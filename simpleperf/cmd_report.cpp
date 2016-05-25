/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include <inttypes.h>
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

#include "command.h"
#include "dwarf_unwind.h"
#include "environment.h"
#include "event_attr.h"
#include "event_type.h"
#include "perf_regs.h"
#include "record.h"
#include "record_file.h"
#include "sample_tree.h"
#include "thread_tree.h"
#include "utils.h"

namespace {

static std::set<std::string> branch_sort_keys = {
    "dso_from", "dso_to", "symbol_from", "symbol_to",
};
struct BranchFromEntry {
  uint64_t ip;
  const MapEntry* map;
  const Symbol* symbol;
  uint64_t flags;

  BranchFromEntry() : ip(0), map(nullptr), symbol(nullptr), flags(0) {}
};

struct SampleEntry {
  uint64_t ip;
  uint64_t time;
  uint64_t period;
  // accumuated when appearing in other sample's callchain
  uint64_t accumulated_period;
  uint64_t sample_count;
  const ThreadEntry* thread;
  const char* thread_comm;
  const MapEntry* map;
  const Symbol* symbol;
  BranchFromEntry branch_from;
  // a callchain tree representing all callchains in the sample
  CallChainRoot<SampleEntry> callchain;

  SampleEntry(uint64_t ip, uint64_t time, uint64_t period,
              uint64_t accumulated_period, uint64_t sample_count,
              const ThreadEntry* thread, const MapEntry* map,
              const Symbol* symbol)
      : ip(ip),
        time(time),
        period(period),
        accumulated_period(accumulated_period),
        sample_count(sample_count),
        thread(thread),
        thread_comm(thread->comm),
        map(map),
        symbol(symbol) {}

  // The data member 'callchain' can only move, not copy.
  SampleEntry(SampleEntry&&) = default;
  SampleEntry(SampleEntry&) = delete;
};

struct SampleTree {
  std::vector<SampleEntry*> samples;
  uint64_t total_samples;
  uint64_t total_period;
};

class ReportCmdSampleTreeBuilder
    : public SampleTreeBuilder<SampleEntry, uint64_t> {
 public:
  ReportCmdSampleTreeBuilder(SampleComparator<SampleEntry> sample_comparator,
                             ThreadTree* thread_tree)
      : SampleTreeBuilder(sample_comparator),
        thread_tree_(thread_tree),
        total_samples_(0),
        total_period_(0) {}

  void SetFilters(const std::unordered_set<int>& pid_filter,
                  const std::unordered_set<int>& tid_filter,
                  const std::unordered_set<std::string>& comm_filter,
                  const std::unordered_set<std::string>& dso_filter) {
    pid_filter_ = pid_filter;
    tid_filter_ = tid_filter;
    comm_filter_ = comm_filter;
    dso_filter_ = dso_filter;
  }

  SampleTree GetSampleTree() const {
    SampleTree sample_tree;
    sample_tree.samples = GetSamples();
    sample_tree.total_samples = total_samples_;
    sample_tree.total_period = total_period_;
    return sample_tree;
  }

 protected:
  SampleEntry* CreateSample(const SampleRecord& r, bool in_kernel,
                            uint64_t* acc_info) override {
    const ThreadEntry* thread =
        thread_tree_->FindThreadOrNew(r.tid_data.pid, r.tid_data.tid);
    const MapEntry* map =
        thread_tree_->FindMap(thread, r.ip_data.ip, in_kernel);
    const Symbol* symbol = thread_tree_->FindSymbol(map, r.ip_data.ip);
    *acc_info = r.period_data.period;
    return InsertSample(std::unique_ptr<SampleEntry>(
        new SampleEntry(r.ip_data.ip, r.time_data.time, r.period_data.period, 0,
                        1, thread, map, symbol)));
  }

  SampleEntry* CreateBranchSample(const SampleRecord& r,
                                  const BranchStackItemType& item) override {
    const ThreadEntry* thread =
        thread_tree_->FindThreadOrNew(r.tid_data.pid, r.tid_data.tid);
    const MapEntry* from_map = thread_tree_->FindMap(thread, item.from);
    const Symbol* from_symbol = thread_tree_->FindSymbol(from_map, item.from);
    const MapEntry* to_map = thread_tree_->FindMap(thread, item.to);
    const Symbol* to_symbol = thread_tree_->FindSymbol(to_map, item.to);
    std::unique_ptr<SampleEntry> sample(
        new SampleEntry(item.to, r.time_data.time, r.period_data.period, 0, 1,
                        thread, to_map, to_symbol));
    sample->branch_from.ip = item.from;
    sample->branch_from.map = from_map;
    sample->branch_from.symbol = from_symbol;
    sample->branch_from.flags = item.flags;
    return InsertSample(std::move(sample));
  }

  SampleEntry* CreateCallChainSample(const SampleEntry* sample, uint64_t ip,
                                     bool in_kernel,
                                     const std::vector<SampleEntry*>& callchain,
                                     const uint64_t& acc_info) override {
    const ThreadEntry* thread = sample->thread;
    const MapEntry* map = thread_tree_->FindMap(thread, ip, in_kernel);
    const Symbol* symbol = thread_tree_->FindSymbol(map, ip);
    std::unique_ptr<SampleEntry> callchain_sample(
        new SampleEntry(ip, sample->time, 0, acc_info, 0, thread, map, symbol));
    return InsertCallChainSample(std::move(callchain_sample), callchain);
  }

  const ThreadEntry* GetThreadOfSample(SampleEntry* sample) override {
    return sample->thread;
  }

  void InsertCallChainForSample(SampleEntry* sample,
                                const std::vector<SampleEntry*>& callchain,
                                const uint64_t& acc_info) override {
    sample->callchain.AddCallChain(callchain, acc_info);
  }

  bool FilterSample(const SampleEntry* sample) override {
    if (!pid_filter_.empty() &&
        pid_filter_.find(sample->thread->pid) == pid_filter_.end()) {
      return false;
    }
    if (!tid_filter_.empty() &&
        tid_filter_.find(sample->thread->tid) == tid_filter_.end()) {
      return false;
    }
    if (!comm_filter_.empty() &&
        comm_filter_.find(sample->thread_comm) == comm_filter_.end()) {
      return false;
    }
    if (!dso_filter_.empty() &&
        dso_filter_.find(sample->map->dso->Path()) == dso_filter_.end()) {
      return false;
    }
    return true;
  }

  void UpdateSummary(const SampleEntry* sample) override {
    total_samples_ += sample->sample_count;
    total_period_ += sample->period;
  }

  void MergeSample(SampleEntry* sample1, SampleEntry* sample2) override {
    sample1->period += sample2->period;
    sample1->accumulated_period += sample2->accumulated_period;
    sample1->sample_count += sample2->sample_count;
  }

 private:
  ThreadTree* thread_tree_;

  std::unordered_set<int> pid_filter_;
  std::unordered_set<int> tid_filter_;
  std::unordered_set<std::string> comm_filter_;
  std::unordered_set<std::string> dso_filter_;

  uint64_t total_samples_;
  uint64_t total_period_;
};

using ReportCmdSampleTreeSorter = SampleTreeSorter<SampleEntry>;
using ReportCmdSampleTreeDisplayer =
    SampleTreeDisplayer<SampleEntry, SampleTree>;

class ReportCommand : public Command {
 public:
  ReportCommand()
      : Command(
            "report", "report sampling information in perf.data",
            // clang-format off
"Usage: simpleperf report [options]\n"
"-b    Use the branch-to addresses in sampled take branches instead of the\n"
"      instruction addresses. Only valid for perf.data recorded with -b/-j\n"
"      option.\n"
"--children    Print the overhead accumulated by appearing in the callchain.\n"
"--comms comm1,comm2,...   Report only for selected comms.\n"
"--dsos dso1,dso2,...      Report only for selected dsos.\n"
"-g [callee|caller]    Print call graph. If callee mode is used, the graph\n"
"                      shows how functions are called from others. Otherwise,\n"
"                      the graph shows how functions call others.\n"
"                      Default is callee mode.\n"
"-i <file>  Specify path of record file, default is perf.data.\n"
"-n         Print the sample count for each item.\n"
"--no-demangle         Don't demangle symbol names.\n"
"-o report_file_name   Set report file name, default is stdout.\n"
"--pids pid1,pid2,...  Report only for selected pids.\n"
"--sort key1,key2,...  Select the keys to sort and print the report.\n"
"                      Possible keys include pid, tid, comm, dso, symbol,\n"
"                      dso_from, dso_to, symbol_from, symbol_to.\n"
"                      dso_from, dso_to, symbol_from, symbol_to can only be\n"
"                      used with -b option.\n"
"                      Default keys are \"comm,pid,tid,dso,symbol\"\n"
"--symfs <dir>         Look for files with symbols relative to this directory.\n"
"--tids tid1,tid2,...  Report only for selected tids.\n"
"--vmlinux <file>      Parse kernel symbols from <file>.\n"
            // clang-format on
            ),
        record_filename_("perf.data"),
        record_file_arch_(GetBuildArch()),
        use_branch_address_(false),
        system_wide_collection_(false),
        accumulate_callchain_(false),
        print_callgraph_(false),
        callgraph_show_callee_(true) {}

  bool Run(const std::vector<std::string>& args);

 private:
  bool ParseOptions(const std::vector<std::string>& args);
  bool ReadEventAttrFromRecordFile();
  bool ReadFeaturesFromRecordFile();
  bool ReadSampleTreeFromRecordFile();
  bool ProcessRecord(std::unique_ptr<Record> record);
  bool PrintReport();
  void PrintReportContext(FILE* fp);

  std::string record_filename_;
  ArchType record_file_arch_;
  std::unique_ptr<RecordFileReader> record_file_reader_;
  std::vector<perf_event_attr> event_attrs_;
  ThreadTree thread_tree_;
  SampleTree sample_tree_;
  std::unique_ptr<ReportCmdSampleTreeBuilder> sample_tree_builder_;
  std::unique_ptr<ReportCmdSampleTreeSorter> sample_tree_sorter_;
  std::unique_ptr<ReportCmdSampleTreeDisplayer> sample_tree_displayer_;
  bool use_branch_address_;
  std::string record_cmdline_;
  bool system_wide_collection_;
  bool accumulate_callchain_;
  bool print_callgraph_;
  bool callgraph_show_callee_;

  std::string report_filename_;
};

bool ReportCommand::Run(const std::vector<std::string>& args) {
  // 1. Parse options.
  if (!ParseOptions(args)) {
    return false;
  }

  // 2. Read record file and build SampleTree.
  record_file_reader_ = RecordFileReader::CreateInstance(record_filename_);
  if (record_file_reader_ == nullptr) {
    return false;
  }
  if (!ReadEventAttrFromRecordFile()) {
    return false;
  }
  // Read features first to prepare build ids used when building SampleTree.
  if (!ReadFeaturesFromRecordFile()) {
    return false;
  }
  ScopedCurrentArch scoped_arch(record_file_arch_);
  if (!ReadSampleTreeFromRecordFile()) {
    return false;
  }

  // 3. Show collected information.
  if (!PrintReport()) {
    return false;
  }

  return true;
}

bool ReportCommand::ParseOptions(const std::vector<std::string>& args) {
  bool demangle = true;
  std::string symfs_dir;
  std::string vmlinux;
  bool print_sample_count = false;
  std::vector<std::string> sort_keys = {"comm", "pid", "tid", "dso", "symbol"};
  std::unordered_set<std::string> comm_filter;
  std::unordered_set<std::string> dso_filter;
  std::unordered_set<int> pid_filter;
  std::unordered_set<int> tid_filter;

  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "-b") {
      use_branch_address_ = true;
    } else if (args[i] == "--children") {
      accumulate_callchain_ = true;
    } else if (args[i] == "--comms" || args[i] == "--dsos") {
      std::unordered_set<std::string>& filter =
          (args[i] == "--comms" ? comm_filter : dso_filter);
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      std::vector<std::string> strs = android::base::Split(args[i], ",");
      filter.insert(strs.begin(), strs.end());

    } else if (args[i] == "-g") {
      print_callgraph_ = true;
      accumulate_callchain_ = true;
      if (i + 1 < args.size() && args[i + 1][0] != '-') {
        ++i;
        if (args[i] == "callee") {
          callgraph_show_callee_ = true;
        } else if (args[i] == "caller") {
          callgraph_show_callee_ = false;
        } else {
          LOG(ERROR) << "Unknown argument with -g option: " << args[i];
          return false;
        }
      }
    } else if (args[i] == "-i") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      record_filename_ = args[i];

    } else if (args[i] == "-n") {
      print_sample_count = true;

    } else if (args[i] == "--no-demangle") {
      demangle = false;
    } else if (args[i] == "-o") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      report_filename_ = args[i];

    } else if (args[i] == "--pids" || args[i] == "--tids") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      std::vector<std::string> strs = android::base::Split(args[i], ",");
      std::vector<int> ids;
      for (const auto& s : strs) {
        int id;
        if (!android::base::ParseInt(s.c_str(), &id, 0)) {
          LOG(ERROR) << "invalid id in " << args[i] << " option: " << s;
          return false;
        }
        ids.push_back(id);
      }
      std::unordered_set<int>& filter =
          (args[i] == "--pids" ? pid_filter : tid_filter);
      filter.insert(ids.begin(), ids.end());

    } else if (args[i] == "--sort") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      sort_keys = android::base::Split(args[i], ",");
    } else if (args[i] == "--symfs") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      symfs_dir = args[i];

    } else if (args[i] == "--vmlinux") {
      if (!NextArgumentOrError(args, &i)) {
        return false;
      }
      vmlinux = args[i];
    } else {
      ReportUnknownOption(args, i);
      return false;
    }
  }

  Dso::SetDemangle(demangle);
  if (!Dso::SetSymFsDir(symfs_dir)) {
    return false;
  }
  if (!vmlinux.empty()) {
    Dso::SetVmlinux(vmlinux);
  }

  SampleDisplayer<SampleEntry, SampleTree> displayer;
  SampleComparator<SampleEntry> comparator;

  if (accumulate_callchain_) {
    displayer.AddDisplayFunction("Children", DisplayAccumulatedOverhead);
    displayer.AddDisplayFunction("Self", DisplaySelfOverhead);
  } else {
    displayer.AddDisplayFunction("Overhead", DisplaySelfOverhead);
  }
  if (print_callgraph_) {
    displayer.AddExclusiveDisplayFunction(DisplayCallgraph);
  }
  if (print_sample_count) {
    displayer.AddDisplayFunction("Sample", DisplaySampleCount);
  }

  for (auto& key : sort_keys) {
    if (!use_branch_address_ &&
        branch_sort_keys.find(key) != branch_sort_keys.end()) {
      LOG(ERROR) << "sort key '" << key << "' can only be used with -b option.";
      return false;
    }
    if (key == "pid") {
      comparator.AddCompareFunction(ComparePid);
      displayer.AddDisplayFunction("Pid", DisplayPid);
    } else if (key == "tid") {
      comparator.AddCompareFunction(CompareTid);
      displayer.AddDisplayFunction("Tid", DisplayTid);
    } else if (key == "comm") {
      comparator.AddCompareFunction(CompareComm);
      displayer.AddDisplayFunction("Command", DisplayComm);
    } else if (key == "dso") {
      comparator.AddCompareFunction(CompareDso);
      displayer.AddDisplayFunction("Shared Object", DisplayDso);
    } else if (key == "symbol") {
      comparator.AddCompareFunction(CompareSymbol);
      displayer.AddDisplayFunction("Symbol", DisplaySymbol);
    } else if (key == "dso_from") {
      comparator.AddCompareFunction(CompareDsoFrom);
      displayer.AddDisplayFunction("Source Shared Object", DisplayDsoFrom);
    } else if (key == "dso_to") {
      comparator.AddCompareFunction(CompareDso);
      displayer.AddDisplayFunction("Target Shared Object", DisplayDso);
    } else if (key == "symbol_from") {
      comparator.AddCompareFunction(CompareSymbolFrom);
      displayer.AddDisplayFunction("Source Symbol", DisplaySymbolFrom);
    } else if (key == "symbol_to") {
      comparator.AddCompareFunction(CompareSymbol);
      displayer.AddDisplayFunction("Target Symbol", DisplaySymbol);
    } else {
      LOG(ERROR) << "Unknown sort key: " << key;
      return false;
    }
  }

  sample_tree_builder_.reset(
      new ReportCmdSampleTreeBuilder(comparator, &thread_tree_));
  sample_tree_builder_->SetFilters(pid_filter, tid_filter, comm_filter,
                                   dso_filter);

  SampleComparator<SampleEntry> sort_comparator;
  sort_comparator.AddCompareFunction(CompareTotalPeriod);
  sort_comparator.AddComparator(comparator);
  sample_tree_sorter_.reset(new ReportCmdSampleTreeSorter(sort_comparator));
  sample_tree_displayer_.reset(new ReportCmdSampleTreeDisplayer(displayer));
  return true;
}

bool ReportCommand::ReadEventAttrFromRecordFile() {
  const std::vector<PerfFileFormat::FileAttr>& file_attrs =
      record_file_reader_->AttrSection();
  for (const auto& attr : file_attrs) {
    event_attrs_.push_back(attr.attr);
  }
  if (use_branch_address_) {
    bool has_branch_stack = true;
    for (const auto& attr : event_attrs_) {
      if ((attr.sample_type & PERF_SAMPLE_BRANCH_STACK) == 0) {
        has_branch_stack = false;
        break;
      }
    }
    if (!has_branch_stack) {
      LOG(ERROR) << record_filename_
                 << " is not recorded with branch stack sampling option.";
      return false;
    }
  }
  return true;
}

bool ReportCommand::ReadFeaturesFromRecordFile() {
  std::vector<BuildIdRecord> records =
      record_file_reader_->ReadBuildIdFeature();
  std::vector<std::pair<std::string, BuildId>> build_ids;
  for (auto& r : records) {
    build_ids.push_back(std::make_pair(r.filename, r.build_id));
  }
  Dso::SetBuildIds(build_ids);

  std::string arch =
      record_file_reader_->ReadFeatureString(PerfFileFormat::FEAT_ARCH);
  if (!arch.empty()) {
    record_file_arch_ = GetArchType(arch);
    if (record_file_arch_ == ARCH_UNSUPPORTED) {
      return false;
    }
  }

  std::vector<std::string> cmdline = record_file_reader_->ReadCmdlineFeature();
  if (!cmdline.empty()) {
    record_cmdline_ = android::base::Join(cmdline, ' ');
    // TODO: the code to detect system wide collection option is fragile, remove
    // it once we can do cross unwinding.
    for (size_t i = 0; i < cmdline.size(); i++) {
      std::string& s = cmdline[i];
      if (s == "-a") {
        system_wide_collection_ = true;
        break;
      } else if (s == "--call-graph" || s == "--cpu" || s == "-e" ||
                 s == "-f" || s == "-F" || s == "-j" || s == "-m" ||
                 s == "-o" || s == "-p" || s == "-t") {
        i++;
      } else if (!s.empty() && s[0] != '-') {
        break;
      }
    }
  }
  return true;
}

bool ReportCommand::ReadSampleTreeFromRecordFile() {
  thread_tree_.AddThread(0, 0, "swapper");
  sample_tree_builder_->SetBranchSampleOption(use_branch_address_);
  // Normally do strict arch check when unwinding stack. But allow unwinding
  // 32-bit processes on 64-bit devices for system wide profiling.
  bool strict_unwind_arch_check = !system_wide_collection_;
  sample_tree_builder_->SetCallChainSampleOptions(
      accumulate_callchain_, print_callgraph_, !callgraph_show_callee_,
      strict_unwind_arch_check);
  if (!record_file_reader_->ReadDataSection(
          [this](std::unique_ptr<Record> record) {
            return ProcessRecord(std::move(record));
          })) {
    return false;
  }
  sample_tree_ = sample_tree_builder_->GetSampleTree();
  sample_tree_sorter_->Sort(sample_tree_.samples, print_callgraph_);
  return true;
}

bool ReportCommand::ProcessRecord(std::unique_ptr<Record> record) {
  BuildThreadTree(*record, &thread_tree_);
  if (record->header.type == PERF_RECORD_SAMPLE) {
    sample_tree_builder_->ProcessSampleRecord(
        *static_cast<const SampleRecord*>(record.get()));
  }
  return true;
}

bool ReportCommand::PrintReport() {
  std::unique_ptr<FILE, decltype(&fclose)> file_handler(nullptr, fclose);
  FILE* report_fp = stdout;
  if (!report_filename_.empty()) {
    report_fp = fopen(report_filename_.c_str(), "w");
    if (report_fp == nullptr) {
      PLOG(ERROR) << "failed to open file " << report_filename_;
      return false;
    }
    file_handler.reset(report_fp);
  }
  PrintReportContext(report_fp);
  sample_tree_displayer_->DisplaySamples(report_fp, sample_tree_.samples,
                                         &sample_tree_);
  fflush(report_fp);
  if (ferror(report_fp) != 0) {
    PLOG(ERROR) << "print report failed";
    return false;
  }
  return true;
}

void ReportCommand::PrintReportContext(FILE* report_fp) {
  if (!record_cmdline_.empty()) {
    fprintf(report_fp, "Cmdline: %s\n", record_cmdline_.c_str());
  }
  for (const auto& attr : event_attrs_) {
    const EventType* event_type = FindEventTypeByConfig(attr.type, attr.config);
    std::string name;
    if (event_type != nullptr) {
      name = event_type->name;
    }
    fprintf(report_fp, "Event: %s (type %u, config %llu)\n", name.c_str(),
            attr.type, attr.config);
  }
  fprintf(report_fp, "Samples: %" PRIu64 "\n", sample_tree_.total_samples);
  fprintf(report_fp, "Event count: %" PRIu64 "\n\n", sample_tree_.total_period);
}

}  // namespace

void RegisterReportCommand() {
  RegisterCommand("report",
                  [] { return std::unique_ptr<Command>(new ReportCommand()); });
}
