/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <stdio.h>

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/scopeguard.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

#include "JITDebugReader.h"
#include "OfflineUnwinder.h"
#include "command.h"
#include "environment.h"
#include "perf_regs.h"
#include "record_file.h"
#include "report_utils.h"
#include "thread_tree.h"
#include "utils.h"

namespace simpleperf {
namespace {

struct MemStat {
  std::string vm_peak;
  std::string vm_size;
  std::string vm_hwm;
  std::string vm_rss;

  std::string ToString() const {
    return android::base::StringPrintf("VmPeak:%s;VmSize:%s;VmHWM:%s;VmRSS:%s", vm_peak.c_str(),
                                       vm_size.c_str(), vm_hwm.c_str(), vm_rss.c_str());
  }
};

static bool GetMemStat(MemStat* stat) {
  std::string s;
  if (!android::base::ReadFileToString(android::base::StringPrintf("/proc/%d/status", getpid()),
                                       &s)) {
    PLOG(ERROR) << "Failed to read process status";
    return false;
  }
  std::vector<std::string> lines = android::base::Split(s, "\n");
  for (auto& line : lines) {
    if (android::base::StartsWith(line, "VmPeak:")) {
      stat->vm_peak = android::base::Trim(line.substr(strlen("VmPeak:")));
    } else if (android::base::StartsWith(line, "VmSize:")) {
      stat->vm_size = android::base::Trim(line.substr(strlen("VmSize:")));
    } else if (android::base::StartsWith(line, "VmHWM:")) {
      stat->vm_hwm = android::base::Trim(line.substr(strlen("VmHWM:")));
    } else if (android::base::StartsWith(line, "VmRSS:")) {
      stat->vm_rss = android::base::Trim(line.substr(strlen("VmRSS:")));
    }
  }
  return true;
}

struct UnwindingStat {
  // For testing unwinding performance
  uint64_t unwinding_sample_count = 0u;
  uint64_t total_unwinding_time_in_ns = 0u;
  uint64_t max_unwinding_time_in_ns = 0u;

  // For memory consumption
  MemStat mem_before_unwinding;
  MemStat mem_after_unwinding;

  void AddUnwindingResult(const UnwindingResult& result) {
    unwinding_sample_count++;
    total_unwinding_time_in_ns += result.used_time;
    max_unwinding_time_in_ns = std::max(max_unwinding_time_in_ns, result.used_time);
  }

  void Dump(FILE* fp) {
    if (unwinding_sample_count == 0) {
      return;
    }
    fprintf(fp, "unwinding_sample_count: %" PRIu64 "\n", unwinding_sample_count);
    fprintf(fp, "average_unwinding_time: %.3f us\n",
            total_unwinding_time_in_ns / 1e3 / unwinding_sample_count);
    fprintf(fp, "max_unwinding_time: %.3f us\n", max_unwinding_time_in_ns / 1e3);

    if (!mem_before_unwinding.vm_peak.empty()) {
      fprintf(fp, "memory_change_VmPeak: %s -> %s\n", mem_before_unwinding.vm_peak.c_str(),
              mem_after_unwinding.vm_peak.c_str());
      fprintf(fp, "memory_change_VmSize: %s -> %s\n", mem_before_unwinding.vm_size.c_str(),
              mem_after_unwinding.vm_size.c_str());
      fprintf(fp, "memory_change_VmHwM: %s -> %s\n", mem_before_unwinding.vm_hwm.c_str(),
              mem_after_unwinding.vm_hwm.c_str());
      fprintf(fp, "memory_change_VmRSS: %s -> %s\n", mem_before_unwinding.vm_rss.c_str(),
              mem_after_unwinding.vm_rss.c_str());
    }
  }
};

class RecordFileProcessor {
 public:
  RecordFileProcessor()
      : unwinder_(OfflineUnwinder::Create(true)), callchain_report_builder_(thread_tree_) {}

  virtual ~RecordFileProcessor() {}

  bool ProcessFile(const std::string& input_filename) {
    // 1. Check input file.
    record_filename_ = input_filename;
    reader_ = RecordFileReader::CreateInstance(record_filename_);
    if (!reader_) {
      return false;
    }
    std::string record_cmd = android::base::Join(reader_->ReadCmdlineFeature(), " ");
    if (record_cmd.find("-g") == std::string::npos &&
        record_cmd.find("--call-graph dwarf") == std::string::npos) {
      LOG(ERROR) << "file isn't recorded with dwarf call graph: " << record_filename_;
      return false;
    }
    if (!CheckRecordCmd(record_cmd)) {
      return false;
    }

    // 2. Load feature sections.
    reader_->LoadBuildIdAndFileFeatures(thread_tree_);
    ScopedCurrentArch scoped_arch(
        GetArchType(reader_->ReadFeatureString(PerfFileFormat::FEAT_ARCH)));
    unwinder_->LoadMetaInfo(reader_->GetMetaInfoFeature());
    if (reader_->HasFeature(PerfFileFormat::FEAT_DEBUG_UNWIND) &&
        reader_->HasFeature(PerfFileFormat::FEAT_DEBUG_UNWIND_FILE)) {
      auto debug_unwind_feature = reader_->ReadDebugUnwindFeature();
      if (!debug_unwind_feature.has_value()) {
        return false;
      }
      uint64_t offset =
          reader_->FeatureSectionDescriptors().at(PerfFileFormat::FEAT_DEBUG_UNWIND_FILE).offset;
      for (DebugUnwindFile& file : debug_unwind_feature.value()) {
        auto& loc = debug_unwind_files_[file.path];
        loc.offset = offset;
        loc.size = file.size;
        offset += file.size;
      }
    }
    callchain_report_builder_.SetRemoveArtFrame(false);
    callchain_report_builder_.SetConvertJITFrame(false);

    // 3. Process records.
    return Process();
  }

 protected:
  struct DebugUnwindFileLocation {
    uint64_t offset;
    uint64_t size;
  };

  virtual bool CheckRecordCmd(const std::string& record_cmd) = 0;
  virtual bool Process() = 0;

  std::string record_filename_;
  std::unique_ptr<RecordFileReader> reader_;
  ThreadTree thread_tree_;
  std::unique_ptr<OfflineUnwinder> unwinder_;
  // Files stored in DEBUG_UNWIND_FILE feature section in the recording file.
  // Map from file path to offset in the recording file.
  std::unordered_map<std::string, DebugUnwindFileLocation> debug_unwind_files_;
  CallChainReportBuilder callchain_report_builder_;
};

static void DumpUnwindingResult(const UnwindingResult& result, FILE* fp) {
  fprintf(fp, "unwinding_used_time: %.3f us\n", result.used_time / 1e3);
  fprintf(fp, "unwinding_error_code: %" PRIu64 "\n", result.error_code);
  fprintf(fp, "unwinding_error_addr: 0x%" PRIx64 "\n", result.error_addr);
  fprintf(fp, "stack_start: 0x%" PRIx64 "\n", result.stack_start);
  fprintf(fp, "stack_end: 0x%" PRIx64 "\n", result.stack_end);
}

class SampleUnwinder : public RecordFileProcessor {
 public:
  SampleUnwinder(const std::string& output_filename, uint64_t sample_time)
      : output_filename_(output_filename), sample_time_(sample_time) {}

 protected:
  bool CheckRecordCmd(const std::string& record_cmd) override {
    if (record_cmd.find("--no-unwind") == std::string::npos &&
        record_cmd.find("--keep-failed-unwinding-debug-info") == std::string::npos) {
      LOG(ERROR) << "file isn't record with --no-unwind or --keep-failed-unwinding-debug-info: "
                 << record_filename_;
      return false;
    }
    return true;
  }

  bool Process() override {
    if (output_filename_.empty()) {
      out_fp_ = stdout;
    } else {
      out_fp_ = fopen(output_filename_.c_str(), "w");
      if (out_fp_ == nullptr) {
        PLOG(ERROR) << "failed to write to " << output_filename_;
        return false;
      }
    }
    auto file_closer = android::base::make_scope_guard([&]() {
      if (out_fp_ != nullptr && out_fp_ != stdout) {
        fclose(out_fp_);
      }
      out_fp_ = nullptr;
    });
    recording_file_dso_ = Dso::CreateDso(DSO_ELF_FILE, record_filename_);

    if (!GetMemStat(&stat_.mem_before_unwinding)) {
      return false;
    }
    if (!reader_->ReadDataSection(
            [&](std::unique_ptr<Record> r) { return ProcessRecord(std::move(r)); })) {
      return false;
    }
    if (!GetMemStat(&stat_.mem_after_unwinding)) {
      return false;
    }
    stat_.Dump(out_fp_);
    return true;
  }

  bool ProcessRecord(std::unique_ptr<Record> r) {
    thread_tree_.Update(*r);
    if (r->type() == SIMPLE_PERF_RECORD_UNWINDING_RESULT) {
      last_unwinding_result_.reset(static_cast<UnwindingResultRecord*>(r.release()));
    } else if (r->type() == PERF_RECORD_SAMPLE) {
      if (sample_time_ == 0 || sample_time_ == r->Timestamp()) {
        auto& sr = *static_cast<SampleRecord*>(r.get());
        const PerfSampleStackUserType* stack = &sr.stack_user_data;
        const PerfSampleRegsUserType* regs = &sr.regs_user_data;
        if (last_unwinding_result_ && last_unwinding_result_->Timestamp() == sr.Timestamp()) {
          stack = &last_unwinding_result_->stack_user_data;
          regs = &last_unwinding_result_->regs_user_data;
        }
        if (stack->size > 0 || regs->reg_mask > 0) {
          if (!UnwindRecord(sr, *regs, *stack)) {
            return false;
          }
        }
      }
      last_unwinding_result_.reset();
    }
    return true;
  }

  bool UnwindRecord(const SampleRecord& r, const PerfSampleRegsUserType& regs,
                    const PerfSampleStackUserType& stack) {
    ThreadEntry* thread = thread_tree_.FindThreadOrNew(r.tid_data.pid, r.tid_data.tid);
    ThreadEntry thread_with_new_maps = CreateThreadWithUpdatedMaps(*thread);

    RegSet reg_set(regs.abi, regs.reg_mask, regs.regs);
    std::vector<uint64_t> ips;
    std::vector<uint64_t> sps;
    if (!unwinder_->UnwindCallChain(thread_with_new_maps, reg_set, stack.data, stack.size, &ips,
                                    &sps)) {
      return false;
    }
    stat_.AddUnwindingResult(unwinder_->GetUnwindingResult());

    // Print unwinding result.
    fprintf(out_fp_, "sample_time: %" PRIu64 "\n", r.Timestamp());
    DumpUnwindingResult(unwinder_->GetUnwindingResult(), out_fp_);
    std::vector<CallChainReportEntry> entries = callchain_report_builder_.Build(thread, ips, 0);
    for (size_t i = 0; i < entries.size(); i++) {
      size_t id = i + 1;
      const auto& entry = entries[i];
      fprintf(out_fp_, "ip_%zu: 0x%" PRIx64 "\n", id, entry.ip);
      fprintf(out_fp_, "sp_%zu: 0x%" PRIx64 "\n", id, sps[i]);
      fprintf(out_fp_, "map_%zu: [0x%" PRIx64 "-0x%" PRIx64 "]\n", id, entry.map->start_addr,
              entry.map->get_end_addr());
      fprintf(out_fp_, "dso_%zu: %s\n", id, entry.map->dso->Path().c_str());
      fprintf(out_fp_, "vaddr_in_file_%zu: 0x%" PRIx64 "\n", id, entry.vaddr_in_file);
      fprintf(out_fp_, "symbol_%zu: %s\n", id, entry.symbol->DemangledName());
    }
    fprintf(out_fp_, "\n");
    return true;
  }

  // To use files stored in DEBUG_UNWIND_FILE feature section, create maps mapping to them.
  ThreadEntry CreateThreadWithUpdatedMaps(const ThreadEntry& thread) {
    ThreadEntry new_thread = thread;
    new_thread.maps.reset(new MapSet);
    new_thread.maps->version = thread.maps->version;
    for (auto& p : thread.maps->maps) {
      const MapEntry* old_map = p.second;
      MapEntry* map = nullptr;
      const std::string& path = old_map->dso->Path();
      if (auto it = debug_unwind_files_.find(path); it != debug_unwind_files_.end()) {
        map_storage_.emplace_back(new MapEntry);
        map = map_storage_.back().get();
        *map = *old_map;
        map->dso = recording_file_dso_.get();
        if (JITDebugReader::IsPathInJITSymFile(old_map->dso->Path())) {
          map->pgoff = it->second.offset;
        } else {
          map->pgoff += it->second.offset;
        }
      } else {
        map = const_cast<MapEntry*>(p.second);
      }
      new_thread.maps->maps[p.first] = map;
    }
    return new_thread;
  }

 private:
  const std::string output_filename_;
  const uint64_t sample_time_;
  std::unique_ptr<Dso> recording_file_dso_;
  std::vector<std::unique_ptr<MapEntry>> map_storage_;
  FILE* out_fp_ = nullptr;
  UnwindingStat stat_;
  std::unique_ptr<UnwindingResultRecord> last_unwinding_result_;
};

class DebugUnwindCommand : public Command {
 public:
  DebugUnwindCommand()
      : Command(
            "debug-unwind", "Debug/test offline unwinding.",
            // clang-format off
"Usage: simpleperf debug-unwind [options]\n"
"-i <file>                 Input recording file. Default is perf.data.\n"
"-o <file>                 Output file. Default is stdout.\n"
"--sample-time <time>      Only process the sample recorded at the selected time.\n"
"--symfs <dir>             Look for files with symbols relative to this directory.\n"
"--unwind-sample           Unwind samples.\n"
"\n"
"Examples:\n"
"1. Unwind a sample.\n"
"$ simpleperf debug-unwind -i perf.data --unwind-sample --sample-time 626970493946976\n"
"  perf.data should be generated with \"--no-unwind\" or \"--keep-failed-unwinding-debug-info\".\n"
"\n"
            // clang-format on
        ) {}

  bool Run(const std::vector<std::string>& args);

 private:
  bool ParseOptions(const std::vector<std::string>& args);

  std::string input_filename_ = "perf.data";
  std::string output_filename_;
  bool unwind_sample_ = false;
  uint64_t sample_time_ = 0;
};

bool DebugUnwindCommand::Run(const std::vector<std::string>& args) {
  // 1. Parse options.
  if (!ParseOptions(args)) {
    return false;
  }

  // 2. Distribute sub commands.
  if (unwind_sample_) {
    SampleUnwinder sample_unwinder(output_filename_, sample_time_);
    return sample_unwinder.ProcessFile(input_filename_);
  }
  return true;
}

bool DebugUnwindCommand::ParseOptions(const std::vector<std::string>& args) {
  const OptionFormatMap option_formats = {
      {"-i", {OptionValueType::STRING, OptionType::SINGLE}},
      {"-o", {OptionValueType::STRING, OptionType::SINGLE}},
      {"--sample-time", {OptionValueType::UINT, OptionType::SINGLE}},
      {"--symfs", {OptionValueType::STRING, OptionType::MULTIPLE}},
      {"--unwind-sample", {OptionValueType::NONE, OptionType::SINGLE}},
  };
  OptionValueMap options;
  std::vector<std::pair<OptionName, OptionValue>> ordered_options;
  if (!PreprocessOptions(args, option_formats, &options, &ordered_options)) {
    return false;
  }
  options.PullStringValue("-i", &input_filename_);
  options.PullStringValue("-o", &output_filename_);
  options.PullUintValue("--sample-time", &sample_time_);
  if (auto value = options.PullValue("--symfs"); value) {
    if (!Dso::SetSymFsDir(*value->str_value)) {
      return false;
    }
  }
  unwind_sample_ = options.PullBoolValue("--unwind-sample");

  CHECK(options.values.empty());
  return true;
}

}  // namespace

void RegisterDebugUnwindCommand() {
  RegisterCommand("debug-unwind",
                  [] { return std::unique_ptr<Command>(new DebugUnwindCommand()); });
}

}  // namespace simpleperf
