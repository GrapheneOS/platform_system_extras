/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>
#include <optional>
#include <utility>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>

#include "JITDebugReader.h"
#include "dso.h"
#include "event_attr.h"
#include "event_type.h"
#include "record_file.h"
#include "report_utils.h"
#include "thread_tree.h"
#include "tracing.h"
#include "utils.h"

extern "C" {

struct Sample {
  uint64_t ip;
  uint32_t pid;
  uint32_t tid;
  const char* thread_comm;
  uint64_t time;
  uint32_t in_kernel;
  uint32_t cpu;
  uint64_t period;
};

struct TracingFieldFormat {
  const char* name;
  uint32_t offset;
  uint32_t elem_size;
  uint32_t elem_count;
  uint32_t is_signed;
  uint32_t is_dynamic;
};

struct TracingDataFormat {
  uint32_t size;
  uint32_t field_count;
  TracingFieldFormat* fields;
};

struct Event {
  const char* name;
  TracingDataFormat tracing_data_format;
};

struct Mapping {
  uint64_t start;
  uint64_t end;
  uint64_t pgoff;
};

struct SymbolEntry {
  const char* dso_name;
  uint64_t vaddr_in_file;
  const char* symbol_name;
  uint64_t symbol_addr;
  uint64_t symbol_len;
  Mapping* mapping;
};

struct CallChainEntry {
  uint64_t ip;
  SymbolEntry symbol;
};

struct CallChain {
  uint32_t nr;
  CallChainEntry* entries;
};

struct FeatureSection {
  const char* data;
  uint32_t data_size;
};

}  // extern "C"

namespace simpleperf {
namespace {

struct EventInfo {
  perf_event_attr attr;
  std::string name;

  struct TracingInfo {
    TracingDataFormat data_format;
    std::vector<std::string> field_names;
    std::vector<TracingFieldFormat> fields;
  } tracing_info;
};

// If a recording file is generated with --trace-offcpu, we can select TraceOffCpuMode to report.
// It affects which samples are reported, and how period in each sample is calculated.
enum class TraceOffCpuMode {
  // Only report on-cpu samples, with period representing events (cpu-cycles or cpu-clock) happened
  // on cpu.
  ON_CPU,
  // Only report off-cpu samples, with period representing time spent off cpu.
  OFF_CPU,
  // Report both on-cpu and off-cpu samples, with period representing time to the next sample.
  ON_OFF_CPU,
};

static std::string TraceOffCpuModeToString(TraceOffCpuMode mode) {
  switch (mode) {
    case TraceOffCpuMode::ON_CPU:
      return "on-cpu";
    case TraceOffCpuMode::OFF_CPU:
      return "off-cpu";
    case TraceOffCpuMode::ON_OFF_CPU:
      return "on-off-cpu";
  }
}

static std::optional<TraceOffCpuMode> StringToTraceOffCpuMode(const std::string& s) {
  if (s == "on-cpu") {
    return TraceOffCpuMode::ON_CPU;
  }
  if (s == "off-cpu") {
    return TraceOffCpuMode::OFF_CPU;
  }
  if (s == "on-off-cpu") {
    return TraceOffCpuMode::ON_OFF_CPU;
  }
  return std::nullopt;
}

struct TraceOffCpuData {
  struct PerThreadData {
    std::unique_ptr<SampleRecord> sr;
    uint64_t switch_out_time = 0;

    void Reset() {
      sr.reset();
      switch_out_time = 0;
    }
  };

  std::vector<TraceOffCpuMode> supported_modes;
  std::string supported_modes_string;
  std::optional<TraceOffCpuMode> mode;
  std::unordered_map<pid_t, PerThreadData> thread_map;
};

}  // namespace

class ReportLib {
 public:
  ReportLib()
      : log_severity_(new android::base::ScopedLogSeverity(android::base::INFO)),
        record_filename_("perf.data"),
        current_thread_(nullptr),
        callchain_report_builder_(thread_tree_) {}

  bool SetLogSeverity(const char* log_level);

  bool SetSymfs(const char* symfs_dir) { return Dso::SetSymFsDir(symfs_dir); }

  bool SetRecordFile(const char* record_file) {
    if (record_file_reader_) {
      LOG(ERROR) << "recording file " << record_filename_ << " has been opened";
      return false;
    }
    record_filename_ = record_file;
    return true;
  }

  bool SetKallsymsFile(const char* kallsyms_file);

  void ShowIpForUnknownSymbol() { thread_tree_.ShowIpForUnknownSymbol(); }
  void ShowArtFrames(bool show) {
    bool remove_art_frame = !show;
    callchain_report_builder_.SetRemoveArtFrame(remove_art_frame);
  }
  void MergeJavaMethods(bool merge) { callchain_report_builder_.SetConvertJITFrame(merge); }
  bool AddProguardMappingFile(const char* mapping_file) {
    return callchain_report_builder_.AddProguardMappingFile(mapping_file);
  }
  const char* GetSupportedTraceOffCpuModes();
  bool SetTraceOffCpuMode(const char* mode);

  Sample* GetNextSample();
  Event* GetEventOfCurrentSample() { return &current_event_; }
  SymbolEntry* GetSymbolOfCurrentSample() { return current_symbol_; }
  CallChain* GetCallChainOfCurrentSample() { return &current_callchain_; }
  const char* GetTracingDataOfCurrentSample() { return current_tracing_data_; }

  const char* GetBuildIdForPath(const char* path);
  FeatureSection* GetFeatureSection(const char* feature_name);

 private:
  Sample* ProcessRecord(std::unique_ptr<Record> r);
  Sample* ProcessRecordForOnCpuSample(std::unique_ptr<Record> r);
  Sample* ProcessRecordForOnOffCpuSample(std::unique_ptr<Record> r);
  Sample* ProcessRecordForOffCpuSample(std::unique_ptr<Record> r);
  void SetCurrentSample(SampleRecord& r, uint64_t period);
  const EventInfo* FindEventOfCurrentSample();
  void CreateEvents();

  bool OpenRecordFileIfNecessary();
  Mapping* AddMapping(const MapEntry& map);

  std::unique_ptr<android::base::ScopedLogSeverity> log_severity_;
  std::string record_filename_;
  std::unique_ptr<RecordFileReader> record_file_reader_;
  ThreadTree thread_tree_;
  std::unique_ptr<SampleRecord> current_record_;
  const ThreadEntry* current_thread_;
  Sample current_sample_;
  Event current_event_;
  SymbolEntry* current_symbol_;
  CallChain current_callchain_;
  const char* current_tracing_data_;
  std::vector<std::unique_ptr<Mapping>> current_mappings_;
  std::vector<CallChainEntry> callchain_entries_;
  std::string build_id_string_;
  std::vector<EventInfo> events_;
  TraceOffCpuData trace_offcpu_;
  FeatureSection feature_section_;
  std::vector<char> feature_section_data_;
  CallChainReportBuilder callchain_report_builder_;
  std::unique_ptr<Tracing> tracing_;
};

bool ReportLib::SetLogSeverity(const char* log_level) {
  android::base::LogSeverity severity;
  if (!GetLogSeverity(log_level, &severity)) {
    LOG(ERROR) << "Unknown log severity: " << log_level;
    return false;
  }
  log_severity_ = nullptr;
  log_severity_.reset(new android::base::ScopedLogSeverity(severity));
  return true;
}

bool ReportLib::SetKallsymsFile(const char* kallsyms_file) {
  std::string kallsyms;
  if (!android::base::ReadFileToString(kallsyms_file, &kallsyms)) {
    LOG(WARNING) << "Failed to read in kallsyms file from " << kallsyms_file;
    return false;
  }
  Dso::SetKallsyms(std::move(kallsyms));
  return true;
}

const char* ReportLib::GetSupportedTraceOffCpuModes() {
  if (!OpenRecordFileIfNecessary()) {
    return nullptr;
  }
  std::string& s = trace_offcpu_.supported_modes_string;
  s.clear();
  for (auto mode : trace_offcpu_.supported_modes) {
    if (!s.empty()) {
      s += ",";
    }
    s += TraceOffCpuModeToString(mode);
  }
  return s.data();
}

bool ReportLib::SetTraceOffCpuMode(const char* mode) {
  auto mode_value = StringToTraceOffCpuMode(mode);
  if (!mode_value) {
    return false;
  }
  if (!OpenRecordFileIfNecessary()) {
    return false;
  }
  auto& modes = trace_offcpu_.supported_modes;
  if (std::find(modes.begin(), modes.end(), mode_value) == modes.end()) {
    return false;
  }
  trace_offcpu_.mode = mode_value;
  return true;
}

bool ReportLib::OpenRecordFileIfNecessary() {
  if (record_file_reader_ == nullptr) {
    record_file_reader_ = RecordFileReader::CreateInstance(record_filename_);
    if (record_file_reader_ == nullptr) {
      return false;
    }
    record_file_reader_->LoadBuildIdAndFileFeatures(thread_tree_);
    auto& meta_info = record_file_reader_->GetMetaInfoFeature();
    if (auto it = meta_info.find("trace_offcpu"); it != meta_info.end() && it->second == "true") {
      // If recorded with --trace-offcpu, default is to report on-off-cpu samples.
      trace_offcpu_.mode = TraceOffCpuMode::ON_OFF_CPU;
      trace_offcpu_.supported_modes.push_back(TraceOffCpuMode::ON_OFF_CPU);
      if (record_file_reader_->AttrSection()[0].attr->context_switch == 1) {
        trace_offcpu_.supported_modes.push_back(TraceOffCpuMode::ON_CPU);
        trace_offcpu_.supported_modes.push_back(TraceOffCpuMode::OFF_CPU);
      }
    }
  }
  return true;
}

Sample* ReportLib::GetNextSample() {
  if (!OpenRecordFileIfNecessary()) {
    return nullptr;
  }
  Sample* sample = nullptr;
  while (sample == nullptr) {
    std::unique_ptr<Record> record;
    if (!record_file_reader_->ReadRecord(record)) {
      return nullptr;
    }
    if (record == nullptr) {
      return nullptr;
    }
    thread_tree_.Update(*record);
    if (record->type() == PERF_RECORD_TRACING_DATA ||
        record->type() == SIMPLE_PERF_RECORD_TRACING_DATA) {
      const auto& r = *static_cast<TracingDataRecord*>(record.get());
      tracing_.reset(new Tracing(std::vector<char>(r.data, r.data + r.data_size)));
    } else {
      sample = ProcessRecord(std::move(record));
    }
  }
  return sample;
}

Sample* ReportLib::ProcessRecord(std::unique_ptr<Record> r) {
  if (!trace_offcpu_.mode || trace_offcpu_.mode == TraceOffCpuMode::ON_CPU) {
    return ProcessRecordForOnCpuSample(std::move(r));
  }
  if (trace_offcpu_.mode == TraceOffCpuMode::ON_OFF_CPU) {
    return ProcessRecordForOnOffCpuSample(std::move(r));
  }
  if (trace_offcpu_.mode == TraceOffCpuMode::OFF_CPU) {
    return ProcessRecordForOffCpuSample(std::move(r));
  }
  return nullptr;
}

Sample* ReportLib::ProcessRecordForOnCpuSample(std::unique_ptr<Record> r) {
  if (r->type() != PERF_RECORD_SAMPLE) {
    return nullptr;
  }
  if (trace_offcpu_.mode == TraceOffCpuMode::ON_CPU) {
    size_t attr_index = record_file_reader_->GetAttrIndexOfRecord(r.get());
    if (attr_index > 0) {
      // Skip samples for sched::sched_switch.
      return nullptr;
    }
  }
  auto sr = static_cast<SampleRecord*>(r.release());
  SetCurrentSample(*sr, sr->period_data.period);
  return &current_sample_;
}

Sample* ReportLib::ProcessRecordForOnOffCpuSample(std::unique_ptr<Record> r) {
  if (r->type() != PERF_RECORD_SAMPLE) {
    return nullptr;
  }
  auto sr = static_cast<SampleRecord*>(r.get());
  uint32_t tid = sr->tid_data.tid;
  auto it = trace_offcpu_.thread_map.find(tid);
  if (it == trace_offcpu_.thread_map.end()) {
    auto& thread_data = trace_offcpu_.thread_map[tid];
    r.release();
    thread_data.sr.reset(sr);
    return nullptr;
  }
  auto& thread_data = it->second;
  SampleRecord* prev_sr = thread_data.sr.release();
  // Use time between two samples as period. The samples can be on-cpu or off-cpu.
  uint64_t period =
      (sr->Timestamp() > prev_sr->Timestamp()) ? (sr->Timestamp() - prev_sr->Timestamp()) : 1;
  r.release();
  thread_data.sr.reset(sr);
  SetCurrentSample(*prev_sr, period);
  return &current_sample_;
}

Sample* ReportLib::ProcessRecordForOffCpuSample(std::unique_ptr<Record> r) {
  // To get off-cpu period, we need to see three consecutive records for the same thread:
  // 1. An off-cpu sample (a sample record for sched:sched_switch event) which is recorded when the
  // thread is scheduled off cpu).
  // 2. A switch/switch_cpu_wide record recorded when the thread is scheduled off cpu.
  // 3. A switch/switch_cpu_wide record recorded when the thread is scheduled on cpu.
  // The time difference between two switch records will be used as off-cpu period for the off-cpu
  // sample.
  // If we don't see these records in order, or if we see on-cpu samples, probably some records are
  // lost. And let's restart the sequence.
  if (r->type() == PERF_RECORD_SAMPLE) {
    auto sr = static_cast<SampleRecord*>(r.get());
    size_t attr_index = record_file_reader_->GetAttrIndexOfRecord(sr);
    bool off_cpu_sample = attr_index > 0;
    uint32_t tid = sr->tid_data.tid;
    auto it = trace_offcpu_.thread_map.find(tid);
    if (it == trace_offcpu_.thread_map.end()) {
      if (off_cpu_sample) {
        auto& thread_data = trace_offcpu_.thread_map[tid];
        r.release();
        thread_data.sr.reset(sr);
      }
    } else {
      auto& thread_data = it->second;
      if (off_cpu_sample) {
        r.release();
        thread_data.sr.reset(sr);
        thread_data.switch_out_time = 0;
      } else {
        // We see an on-cpu sample, restart the sequence.
        thread_data.Reset();
      }
    }
    return nullptr;
  }
  if (r->type() == PERF_RECORD_SWITCH || r->type() == PERF_RECORD_SWITCH_CPU_WIDE) {
    uint32_t tid = r->sample_id.tid_data.tid;
    bool switch_out = r->header.misc & PERF_RECORD_MISC_SWITCH_OUT;
    auto it = trace_offcpu_.thread_map.find(tid);
    if (it == trace_offcpu_.thread_map.end()) {
      return nullptr;
    }
    auto& thread_data = it->second;
    if (!thread_data.sr) {
      return nullptr;
    }
    if (thread_data.switch_out_time == 0) {
      // We need a switch out record.
      if (switch_out) {
        thread_data.switch_out_time = r->Timestamp();
      } else {
        thread_data.Reset();
      }
    } else {
      // We need a switch in record.
      if (switch_out) {
        thread_data.Reset();
      } else {
        uint64_t period = (r->Timestamp() > thread_data.switch_out_time)
                              ? (r->Timestamp() - thread_data.switch_out_time)
                              : 1;
        SetCurrentSample(*thread_data.sr.release(), period);
        thread_data.Reset();
        return &current_sample_;
      }
    }
  }
  return nullptr;
}

void ReportLib::SetCurrentSample(SampleRecord& r, uint64_t period) {
  current_record_.reset(&r);
  current_mappings_.clear();
  callchain_entries_.clear();
  current_sample_.ip = r.ip_data.ip;
  current_sample_.pid = r.tid_data.pid;
  current_sample_.tid = r.tid_data.tid;
  current_thread_ = thread_tree_.FindThreadOrNew(r.tid_data.pid, r.tid_data.tid);
  current_sample_.thread_comm = current_thread_->comm;
  current_sample_.time = r.time_data.time;
  current_sample_.in_kernel = r.InKernel();
  current_sample_.cpu = r.cpu_data.cpu;
  current_sample_.period = period;

  size_t kernel_ip_count;
  std::vector<uint64_t> ips = r.GetCallChain(&kernel_ip_count);
  std::vector<CallChainReportEntry> report_entries =
      callchain_report_builder_.Build(current_thread_, ips, kernel_ip_count);

  for (const auto& report_entry : report_entries) {
    callchain_entries_.resize(callchain_entries_.size() + 1);
    CallChainEntry& entry = callchain_entries_.back();
    entry.ip = report_entry.ip;
    if (report_entry.dso_name != nullptr) {
      entry.symbol.dso_name = report_entry.dso_name;
    } else {
      entry.symbol.dso_name = report_entry.dso->GetReportPath().data();
    }
    entry.symbol.vaddr_in_file = report_entry.vaddr_in_file;
    entry.symbol.symbol_name = report_entry.symbol->DemangledName();
    entry.symbol.symbol_addr = report_entry.symbol->addr;
    entry.symbol.symbol_len = report_entry.symbol->len;
    entry.symbol.mapping = AddMapping(*report_entry.map);
  }
  current_sample_.ip = callchain_entries_[0].ip;
  current_symbol_ = &(callchain_entries_[0].symbol);
  current_callchain_.nr = callchain_entries_.size() - 1;
  current_callchain_.entries = &callchain_entries_[1];
  const EventInfo* event = FindEventOfCurrentSample();
  current_event_.name = event->name.c_str();
  current_event_.tracing_data_format = event->tracing_info.data_format;
  if (current_event_.tracing_data_format.size > 0u && (r.sample_type & PERF_SAMPLE_RAW)) {
    CHECK_GE(r.raw_data.size, current_event_.tracing_data_format.size);
    current_tracing_data_ = r.raw_data.data;
  } else {
    current_tracing_data_ = nullptr;
  }
}

const EventInfo* ReportLib::FindEventOfCurrentSample() {
  if (events_.empty()) {
    CreateEvents();
  }
  size_t attr_index;
  if (trace_offcpu_.mode == TraceOffCpuMode::ON_OFF_CPU) {
    // When report both on-cpu and off-cpu samples, pretend they are from the same event type.
    // Otherwise, some report scripts may split them.
    attr_index = 0;
  } else {
    attr_index = record_file_reader_->GetAttrIndexOfRecord(current_record_.get());
  }
  return &events_[attr_index];
}

void ReportLib::CreateEvents() {
  std::vector<EventAttrWithId> attrs = record_file_reader_->AttrSection();
  events_.resize(attrs.size());
  for (size_t i = 0; i < attrs.size(); ++i) {
    events_[i].attr = *attrs[i].attr;
    events_[i].name = GetEventNameByAttr(events_[i].attr);
    EventInfo::TracingInfo& tracing_info = events_[i].tracing_info;
    if (events_[i].attr.type == PERF_TYPE_TRACEPOINT && tracing_) {
      TracingFormat format = tracing_->GetTracingFormatHavingId(events_[i].attr.config);
      tracing_info.field_names.resize(format.fields.size());
      tracing_info.fields.resize(format.fields.size());
      for (size_t i = 0; i < format.fields.size(); ++i) {
        tracing_info.field_names[i] = format.fields[i].name;
        TracingFieldFormat& field = tracing_info.fields[i];
        field.name = tracing_info.field_names[i].c_str();
        field.offset = format.fields[i].offset;
        field.elem_size = format.fields[i].elem_size;
        field.elem_count = format.fields[i].elem_count;
        field.is_signed = format.fields[i].is_signed;
        field.is_dynamic = format.fields[i].is_dynamic;
      }
      if (tracing_info.fields.empty()) {
        tracing_info.data_format.size = 0;
      } else {
        TracingFieldFormat& field = tracing_info.fields.back();
        tracing_info.data_format.size = field.offset + field.elem_size * field.elem_count;
      }
      tracing_info.data_format.field_count = tracing_info.fields.size();
      tracing_info.data_format.fields = &tracing_info.fields[0];
    } else {
      tracing_info.data_format.size = 0;
      tracing_info.data_format.field_count = 0;
      tracing_info.data_format.fields = nullptr;
    }
  }
}

Mapping* ReportLib::AddMapping(const MapEntry& map) {
  current_mappings_.emplace_back(std::unique_ptr<Mapping>(new Mapping));
  Mapping* mapping = current_mappings_.back().get();
  mapping->start = map.start_addr;
  mapping->end = map.start_addr + map.len;
  mapping->pgoff = map.pgoff;
  return mapping;
}

const char* ReportLib::GetBuildIdForPath(const char* path) {
  if (!OpenRecordFileIfNecessary()) {
    build_id_string_.clear();
    return build_id_string_.c_str();
  }
  BuildId build_id = Dso::FindExpectedBuildIdForPath(path);
  if (build_id.IsEmpty()) {
    build_id_string_.clear();
  } else {
    build_id_string_ = build_id.ToString();
  }
  return build_id_string_.c_str();
}

FeatureSection* ReportLib::GetFeatureSection(const char* feature_name) {
  if (!OpenRecordFileIfNecessary()) {
    return nullptr;
  }
  int feature = PerfFileFormat::GetFeatureId(feature_name);
  if (feature == -1 || !record_file_reader_->ReadFeatureSection(feature, &feature_section_data_)) {
    return nullptr;
  }
  feature_section_.data = feature_section_data_.data();
  feature_section_.data_size = feature_section_data_.size();
  return &feature_section_;
}

}  // namespace simpleperf

using ReportLib = simpleperf::ReportLib;

extern "C" {

#define EXPORT __attribute__((visibility("default")))

// Create a new instance,
// pass the instance to the other functions below.
ReportLib* CreateReportLib() EXPORT;
void DestroyReportLib(ReportLib* report_lib) EXPORT;

// Set log severity, different levels are:
// verbose, debug, info, warning, error, fatal.
bool SetLogSeverity(ReportLib* report_lib, const char* log_level) EXPORT;
bool SetSymfs(ReportLib* report_lib, const char* symfs_dir) EXPORT;
bool SetRecordFile(ReportLib* report_lib, const char* record_file) EXPORT;
bool SetKallsymsFile(ReportLib* report_lib, const char* kallsyms_file) EXPORT;
void ShowIpForUnknownSymbol(ReportLib* report_lib) EXPORT;
void ShowArtFrames(ReportLib* report_lib, bool show) EXPORT;
void MergeJavaMethods(ReportLib* report_lib, bool merge) EXPORT;
bool AddProguardMappingFile(ReportLib* report_lib, const char* mapping_file) EXPORT;
const char* GetSupportedTraceOffCpuModes(ReportLib* report_lib) EXPORT;
bool SetTraceOffCpuMode(ReportLib* report_lib, const char* mode) EXPORT;

Sample* GetNextSample(ReportLib* report_lib) EXPORT;
Event* GetEventOfCurrentSample(ReportLib* report_lib) EXPORT;
SymbolEntry* GetSymbolOfCurrentSample(ReportLib* report_lib) EXPORT;
CallChain* GetCallChainOfCurrentSample(ReportLib* report_lib) EXPORT;
const char* GetTracingDataOfCurrentSample(ReportLib* report_lib) EXPORT;

const char* GetBuildIdForPath(ReportLib* report_lib, const char* path) EXPORT;
FeatureSection* GetFeatureSection(ReportLib* report_lib, const char* feature_name) EXPORT;
}

// Exported methods working with a client created instance
ReportLib* CreateReportLib() {
  return new ReportLib();
}

void DestroyReportLib(ReportLib* report_lib) {
  delete report_lib;
}

bool SetLogSeverity(ReportLib* report_lib, const char* log_level) {
  return report_lib->SetLogSeverity(log_level);
}

bool SetSymfs(ReportLib* report_lib, const char* symfs_dir) {
  return report_lib->SetSymfs(symfs_dir);
}

bool SetRecordFile(ReportLib* report_lib, const char* record_file) {
  return report_lib->SetRecordFile(record_file);
}

void ShowIpForUnknownSymbol(ReportLib* report_lib) {
  return report_lib->ShowIpForUnknownSymbol();
}

void ShowArtFrames(ReportLib* report_lib, bool show) {
  return report_lib->ShowArtFrames(show);
}

void MergeJavaMethods(ReportLib* report_lib, bool merge) {
  return report_lib->MergeJavaMethods(merge);
}

bool SetKallsymsFile(ReportLib* report_lib, const char* kallsyms_file) {
  return report_lib->SetKallsymsFile(kallsyms_file);
}

bool AddProguardMappingFile(ReportLib* report_lib, const char* mapping_file) {
  return report_lib->AddProguardMappingFile(mapping_file);
}

const char* GetSupportedTraceOffCpuModes(ReportLib* report_lib) {
  return report_lib->GetSupportedTraceOffCpuModes();
}

bool SetTraceOffCpuMode(ReportLib* report_lib, const char* mode) {
  return report_lib->SetTraceOffCpuMode(mode);
}

Sample* GetNextSample(ReportLib* report_lib) {
  return report_lib->GetNextSample();
}

Event* GetEventOfCurrentSample(ReportLib* report_lib) {
  return report_lib->GetEventOfCurrentSample();
}

SymbolEntry* GetSymbolOfCurrentSample(ReportLib* report_lib) {
  return report_lib->GetSymbolOfCurrentSample();
}

CallChain* GetCallChainOfCurrentSample(ReportLib* report_lib) {
  return report_lib->GetCallChainOfCurrentSample();
}

const char* GetTracingDataOfCurrentSample(ReportLib* report_lib) {
  return report_lib->GetTracingDataOfCurrentSample();
}

const char* GetBuildIdForPath(ReportLib* report_lib, const char* path) {
  return report_lib->GetBuildIdForPath(path);
}

FeatureSection* GetFeatureSection(ReportLib* report_lib, const char* feature_name) {
  return report_lib->GetFeatureSection(feature_name);
}
