/*
 * Copyright (C) 2019 The Android Open Source Project
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
#include <unistd.h>

#include <memory>
#include <optional>
#include <regex>
#include <string>

#include <android-base/parseint.h>
#include <android-base/strings.h>

#include "ETMDecoder.h"
#include "cmd_inject_impl.h"
#include "command.h"
#include "record_file.h"
#include "system/extras/simpleperf/etm_branch_list.pb.h"
#include "thread_tree.h"
#include "utils.h"

namespace simpleperf {

std::string BranchToProtoString(const std::vector<bool>& branch) {
  size_t bytes = (branch.size() + 7) / 8;
  std::string res(bytes, '\0');
  for (size_t i = 0; i < branch.size(); i++) {
    if (branch[i]) {
      res[i >> 3] |= 1 << (i & 7);
    }
  }
  return res;
}

std::vector<bool> ProtoStringToBranch(const std::string& s, size_t bit_size) {
  std::vector<bool> branch(bit_size, false);
  for (size_t i = 0; i < bit_size; i++) {
    if (s[i >> 3] & (1 << (i & 7))) {
      branch[i] = true;
    }
  }
  return branch;
}

namespace {

constexpr const char* ETM_BRANCH_LIST_PROTO_MAGIC = "simpleperf:EtmBranchList";

using AddrPair = std::pair<uint64_t, uint64_t>;

struct AddrPairHash {
  size_t operator()(const AddrPair& ap) const noexcept {
    size_t seed = 0;
    HashCombine(seed, ap.first);
    HashCombine(seed, ap.second);
    return seed;
  }
};

enum class OutputFormat {
  AutoFDO,
  BranchList,
};

// We identify a binary by its path and build_id. kernel_start_addr is also used for vmlinux.
// Because it affects how addresses in BranchListBinaryInfo are interpreted.
struct BinaryKey {
  std::string path;
  BuildId build_id;
  uint64_t kernel_start_addr = 0;

  BinaryKey() {}

  BinaryKey(Dso* dso, uint64_t kernel_start_addr) : path(dso->Path()) {
    build_id = Dso::FindExpectedBuildIdForPath(dso->Path());
    if (dso->type() == DSO_KERNEL) {
      this->kernel_start_addr = kernel_start_addr;
    }
  }

  bool operator==(const BinaryKey& other) const {
    return path == other.path && build_id == other.build_id &&
           kernel_start_addr == other.kernel_start_addr;
  }
};

struct BinaryKeyHash {
  size_t operator()(const BinaryKey& key) const noexcept {
    size_t seed = 0;
    HashCombine(seed, key.path);
    HashCombine(seed, key.build_id);
    if (key.kernel_start_addr != 0) {
      HashCombine(seed, key.kernel_start_addr);
    }
    return seed;
  }
};

struct AutoFDOBinaryInfo {
  uint64_t first_load_segment_addr = 0;
  std::unordered_map<AddrPair, uint64_t, AddrPairHash> range_count_map;
  std::unordered_map<AddrPair, uint64_t, AddrPairHash> branch_count_map;

  void Merge(const AutoFDOBinaryInfo& other) {
    for (const auto& p : other.range_count_map) {
      auto res = range_count_map.emplace(p.first, p.second);
      if (!res.second) {
        res.first->second += p.second;
      }
    }
    for (const auto& p : other.branch_count_map) {
      auto res = branch_count_map.emplace(p.first, p.second);
      if (!res.second) {
        res.first->second += p.second;
      }
    }
  }
};

using UnorderedBranchMap =
    std::unordered_map<uint64_t, std::unordered_map<std::vector<bool>, uint64_t>>;

struct BranchListBinaryInfo {
  DsoType dso_type;
  UnorderedBranchMap branch_map;

  void Merge(const BranchListBinaryInfo& other) {
    for (auto& other_p : other.branch_map) {
      auto it = branch_map.find(other_p.first);
      if (it == branch_map.end()) {
        branch_map[other_p.first] = std::move(other_p.second);
      } else {
        auto& map2 = it->second;
        for (auto& other_p2 : other_p.second) {
          auto it2 = map2.find(other_p2.first);
          if (it2 == map2.end()) {
            map2[other_p2.first] = other_p2.second;
          } else {
            if (__builtin_add_overflow(it2->second, other_p2.second, &it2->second)) {
              it2->second = UINT64_MAX;
            }
          }
        }
      }
    }
  }
};

class ThreadTreeWithFilter : public ThreadTree {
 public:
  void ExcludePid(pid_t pid) { exclude_pid_ = pid; }

  ThreadEntry* FindThread(int tid) const override {
    ThreadEntry* thread = ThreadTree::FindThread(tid);
    if (thread != nullptr && exclude_pid_ && thread->pid == exclude_pid_) {
      return nullptr;
    }
    return thread;
  }

 private:
  std::optional<pid_t> exclude_pid_;
};

// Write instruction ranges to a file in AutoFDO text format.
class AutoFDOWriter {
 public:
  void AddAutoFDOBinary(const BinaryKey& key, AutoFDOBinaryInfo& binary) {
    auto it = binary_map_.find(key);
    if (it == binary_map_.end()) {
      binary_map_[key] = std::move(binary);
    } else {
      it->second.Merge(binary);
    }
  }

  bool Write(const std::string& output_filename) {
    std::unique_ptr<FILE, decltype(&fclose)> output_fp(fopen(output_filename.c_str(), "w"), fclose);
    if (!output_fp) {
      PLOG(ERROR) << "failed to write to " << output_filename;
      return false;
    }
    // autofdo_binary_map is used to store instruction ranges, which can have a large amount. And
    // it has a larger access time (instruction ranges * executed time). So it's better to use
    // unorder_maps to speed up access time. But we also want a stable output here, to compare
    // output changes result from code changes. So generate a sorted output here.
    std::vector<BinaryKey> keys;
    for (auto& p : binary_map_) {
      keys.emplace_back(p.first);
    }
    std::sort(keys.begin(), keys.end(),
              [](const BinaryKey& key1, const BinaryKey& key2) { return key1.path < key2.path; });
    if (keys.size() > 1) {
      fprintf(output_fp.get(),
              "// Please split this file. AutoFDO only accepts profile for one binary.\n");
    }
    for (const auto& key : keys) {
      const AutoFDOBinaryInfo& binary = binary_map_[key];
      // AutoFDO text format needs file_offsets instead of virtual addrs in a binary. And it uses
      // below formula: vaddr = file_offset + GetFirstLoadSegmentVaddr().
      uint64_t first_load_segment_addr = binary.first_load_segment_addr;

      auto to_offset = [&](uint64_t vaddr) -> uint64_t {
        if (vaddr == 0) {
          return 0;
        }
        CHECK_GE(vaddr, first_load_segment_addr);
        return vaddr - first_load_segment_addr;
      };

      // Write range_count_map.
      std::map<AddrPair, uint64_t> range_count_map(binary.range_count_map.begin(),
                                                   binary.range_count_map.end());
      fprintf(output_fp.get(), "%zu\n", range_count_map.size());
      for (const auto& pair2 : range_count_map) {
        const AddrPair& addr_range = pair2.first;
        uint64_t count = pair2.second;

        fprintf(output_fp.get(), "%" PRIx64 "-%" PRIx64 ":%" PRIu64 "\n",
                to_offset(addr_range.first), to_offset(addr_range.second), count);
      }

      // Write addr_count_map.
      fprintf(output_fp.get(), "0\n");

      // Write branch_count_map.
      std::map<AddrPair, uint64_t> branch_count_map(binary.branch_count_map.begin(),
                                                    binary.branch_count_map.end());
      fprintf(output_fp.get(), "%zu\n", branch_count_map.size());
      for (const auto& pair2 : branch_count_map) {
        const AddrPair& branch = pair2.first;
        uint64_t count = pair2.second;

        fprintf(output_fp.get(), "%" PRIx64 "->%" PRIx64 ":%" PRIu64 "\n", to_offset(branch.first),
                to_offset(branch.second), count);
      }

      // Write the binary path in comment.
      fprintf(output_fp.get(), "// %s\n\n", key.path.c_str());
    }
    return true;
  }

 private:
  std::unordered_map<BinaryKey, AutoFDOBinaryInfo, BinaryKeyHash> binary_map_;
};

// Write branch lists to a protobuf file specified by etm_branch_list.proto.
class BranchListWriter {
 public:
  void AddBranchListBinary(const BinaryKey& key, BranchListBinaryInfo& binary) {
    auto it = binary_map_.find(key);
    if (it == binary_map_.end()) {
      binary_map_[key] = std::move(binary);
    } else {
      it->second.Merge(binary);
    }
  }

  bool Write(const std::string& output_filename) {
    // Don't produce empty output file.
    if (binary_map_.empty()) {
      LOG(INFO) << "Skip empty output file.";
      unlink(output_filename.c_str());
      return true;
    }
    std::unique_ptr<FILE, decltype(&fclose)> output_fp(fopen(output_filename.c_str(), "wb"),
                                                       fclose);
    if (!output_fp) {
      PLOG(ERROR) << "failed to write to " << output_filename;
      return false;
    }

    proto::ETMBranchList branch_list_proto;
    branch_list_proto.set_magic(ETM_BRANCH_LIST_PROTO_MAGIC);
    std::vector<char> branch_buf;
    for (const auto& p : binary_map_) {
      const BinaryKey& key = p.first;
      const BranchListBinaryInfo& binary = p.second;
      auto binary_proto = branch_list_proto.add_binaries();

      binary_proto->set_path(key.path);
      if (!key.build_id.IsEmpty()) {
        binary_proto->set_build_id(key.build_id.ToString().substr(2));
      }
      auto opt_binary_type = ToProtoBinaryType(binary.dso_type);
      if (!opt_binary_type.has_value()) {
        return false;
      }
      binary_proto->set_type(opt_binary_type.value());

      for (const auto& addr_p : binary.branch_map) {
        auto addr_proto = binary_proto->add_addrs();
        addr_proto->set_addr(addr_p.first);

        for (const auto& branch_p : addr_p.second) {
          const std::vector<bool>& branch = branch_p.first;
          auto branch_proto = addr_proto->add_branches();

          branch_proto->set_branch(BranchToProtoString(branch));
          branch_proto->set_branch_size(branch.size());
          branch_proto->set_count(branch_p.second);
        }
      }

      if (binary.dso_type == DSO_KERNEL) {
        binary_proto->mutable_kernel_info()->set_kernel_start_addr(key.kernel_start_addr);
      }
    }
    if (!branch_list_proto.SerializeToFileDescriptor(fileno(output_fp.get()))) {
      PLOG(ERROR) << "failed to write to " << output_filename;
      return false;
    }
    return true;
  }

 private:
  std::optional<proto::ETMBranchList_Binary::BinaryType> ToProtoBinaryType(DsoType dso_type) {
    switch (dso_type) {
      case DSO_ELF_FILE:
        return proto::ETMBranchList_Binary::ELF_FILE;
      case DSO_KERNEL:
        return proto::ETMBranchList_Binary::KERNEL;
      case DSO_KERNEL_MODULE:
        return proto::ETMBranchList_Binary::KERNEL_MODULE;
      default:
        LOG(ERROR) << "unexpected dso type " << dso_type;
        return std::nullopt;
    }
  }

  std::unordered_map<BinaryKey, BranchListBinaryInfo, BinaryKeyHash> binary_map_;
};  // namespace

class InjectCommand : public Command {
 public:
  InjectCommand()
      : Command("inject", "parse etm instruction tracing data",
                // clang-format off
"Usage: simpleperf inject [options]\n"
"--binary binary_name         Generate data only for binaries matching binary_name regex.\n"
"-i file1,file2,...           Input files. Default is perf.data. Support below formats:\n"
"                               1. perf.data generated by recording cs-etm event type.\n"
"                               2. branch_list file generated by `inject --output branch-list`.\n"
"                             If a file name starts with @, it contains a list of input files.\n"
"-o <file>                    output file. Default is perf_inject.data.\n"
"--output <format>            Select output file format:\n"
"                               autofdo      -- text format accepted by TextSampleReader\n"
"                                               of AutoFDO\n"
"                               branch-list  -- protobuf file in etm_branch_list.proto\n"
"                             Default is autofdo.\n"
"--dump-etm type1,type2,...   Dump etm data. A type is one of raw, packet and element.\n"
"--exclude-perf               Exclude trace data for the recording process.\n"
"--symdir <dir>               Look for binaries in a directory recursively.\n"
"\n"
"Examples:\n"
"1. Generate autofdo text output.\n"
"$ simpleperf inject -i perf.data -o autofdo.txt --output autofdo\n"
"\n"
"2. Generate branch list proto, then convert to autofdo text.\n"
"$ simpleperf inject -i perf.data -o branch_list.data --output branch-list\n"
"$ simpleperf inject -i branch_list.data -o autofdo.txt --output autofdo\n"
                // clang-format on
        ) {}

  bool Run(const std::vector<std::string>& args) override {
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    if (!ParseOptions(args)) {
      return false;
    }

    for (const auto& filename : input_filenames_) {
      if (!ProcessInputFile(filename)) {
        return false;
      }
    }

    return WriteOutput();
  }

 private:
  bool ParseOptions(const std::vector<std::string>& args) {
    const OptionFormatMap option_formats = {
        {"--binary", {OptionValueType::STRING, OptionType::SINGLE}},
        {"--dump-etm", {OptionValueType::STRING, OptionType::SINGLE}},
        {"--exclude-perf", {OptionValueType::NONE, OptionType::SINGLE}},
        {"-i", {OptionValueType::STRING, OptionType::MULTIPLE}},
        {"-o", {OptionValueType::STRING, OptionType::SINGLE}},
        {"--output", {OptionValueType::STRING, OptionType::SINGLE}},
        {"--symdir", {OptionValueType::STRING, OptionType::MULTIPLE}},
    };
    OptionValueMap options;
    std::vector<std::pair<OptionName, OptionValue>> ordered_options;
    if (!PreprocessOptions(args, option_formats, &options, &ordered_options, nullptr)) {
      return false;
    }

    if (auto value = options.PullValue("--binary"); value) {
      binary_name_regex_ = *value->str_value;
    }
    if (auto value = options.PullValue("--dump-etm"); value) {
      if (!ParseEtmDumpOption(*value->str_value, &etm_dump_option_)) {
        return false;
      }
    }
    exclude_perf_ = options.PullBoolValue("--exclude-perf");

    for (const OptionValue& value : options.PullValues("-i")) {
      std::vector<std::string> files = android::base::Split(*value.str_value, ",");
      for (std::string& file : files) {
        if (android::base::StartsWith(file, "@")) {
          if (!ReadFileList(file.substr(1), &input_filenames_)) {
            return false;
          }
        } else {
          input_filenames_.emplace_back(file);
        }
      }
    }
    if (input_filenames_.empty()) {
      input_filenames_.emplace_back("perf.data");
    }
    options.PullStringValue("-o", &output_filename_);
    if (auto value = options.PullValue("--output"); value) {
      const std::string& output = *value->str_value;
      if (output == "autofdo") {
        output_format_ = OutputFormat::AutoFDO;
      } else if (output == "branch-list") {
        output_format_ = OutputFormat::BranchList;
      } else {
        LOG(ERROR) << "unknown format in --output option: " << output;
        return false;
      }
    }
    if (auto value = options.PullValue("--symdir"); value) {
      if (!Dso::AddSymbolDir(*value->str_value)) {
        return false;
      }
    }
    CHECK(options.values.empty());
    return true;
  }

  bool ReadFileList(const std::string& path, std::vector<std::string>* file_list) {
    std::string data;
    if (!android::base::ReadFileToString(path, &data)) {
      PLOG(ERROR) << "failed to read " << path;
      return false;
    }
    std::vector<std::string> tokens = android::base::Tokenize(data, " \t\n\r");
    file_list->insert(file_list->end(), tokens.begin(), tokens.end());
    return true;
  }

  bool ProcessInputFile(const std::string& input_filename) {
    if (IsPerfDataFile(input_filename)) {
      record_file_reader_ = RecordFileReader::CreateInstance(input_filename);
      if (!record_file_reader_) {
        return false;
      }
      if (exclude_perf_) {
        const auto& info_map = record_file_reader_->GetMetaInfoFeature();
        if (auto it = info_map.find("recording_process"); it == info_map.end()) {
          LOG(ERROR) << input_filename << " doesn't support --exclude-perf";
          return false;
        } else {
          int pid;
          if (!android::base::ParseInt(it->second, &pid, 0)) {
            LOG(ERROR) << "invalid recording_process " << it->second;
            return false;
          }
          thread_tree_.ExcludePid(pid);
        }
      }
      record_file_reader_->LoadBuildIdAndFileFeatures(thread_tree_);
      if (!record_file_reader_->ReadDataSection(
              [this](auto r) { return ProcessRecord(r.get()); })) {
        return false;
      }
      if (etm_decoder_ && !etm_decoder_->FinishData()) {
        return false;
      }
    } else {
      if (!ProcessBranchListFile(input_filename)) {
        return false;
      }
    }
    PostProcessInputFile();
    return true;
  }

  void PostProcessInputFile() {
    // When processing binary info in an input file, the binaries are identified by their path.
    // But this isn't sufficient when merging binary info from multiple input files. Because
    // binaries for the same path may be changed between generating input files. So after processing
    // each input file, we create BinaryKeys to identify binaries, which consider path, build_id and
    // kernel_start_addr (for vmlinux).
    if (output_format_ == OutputFormat::AutoFDO) {
      for (auto& p : autofdo_binary_map_) {
        Dso* dso = p.first;
        AutoFDOBinaryInfo& binary = p.second;
        binary.first_load_segment_addr = GetFirstLoadSegmentVaddr(dso);
        autofdo_writer_.AddAutoFDOBinary(BinaryKey(dso, 0), binary);
      }
      autofdo_binary_map_.clear();
      return;
    }
    CHECK(output_format_ == OutputFormat::BranchList);
    for (auto& p : branch_list_binary_map_) {
      Dso* dso = p.first;
      BranchListBinaryInfo& binary = p.second;
      binary.dso_type = dso->type();
      BinaryKey key(dso, 0);
      if (binary.dso_type == DSO_KERNEL) {
        if (kernel_map_start_addr_ == 0) {
          LOG(WARNING) << "Can't convert kernel ip addresses without kernel start addr. So remove "
                          "branches for the kernel.";
          continue;
        }
        if (dso->GetDebugFilePath() == dso->Path()) {
          // vmlinux isn't available. We still use kernel ip addr. Put kernel start addr in proto
          // for address conversion later.
          key.kernel_start_addr = kernel_map_start_addr_;
        }
      }
      branch_list_writer_.AddBranchListBinary(key, binary);
    }
    kernel_map_start_addr_ = 0;
    branch_list_binary_map_.clear();
  }

  bool ProcessRecord(Record* r) {
    thread_tree_.Update(*r);
    if (r->type() == PERF_RECORD_AUXTRACE_INFO) {
      etm_decoder_ = ETMDecoder::Create(*static_cast<AuxTraceInfoRecord*>(r), thread_tree_);
      if (!etm_decoder_) {
        return false;
      }
      etm_decoder_->EnableDump(etm_dump_option_);
      if (output_format_ == OutputFormat::AutoFDO) {
        etm_decoder_->RegisterCallback(
            [this](const ETMInstrRange& range) { ProcessInstrRange(range); });
      } else if (output_format_ == OutputFormat::BranchList) {
        etm_decoder_->RegisterCallback(
            [this](const ETMBranchList& branch) { ProcessBranchList(branch); });
      }
    } else if (r->type() == PERF_RECORD_AUX) {
      AuxRecord* aux = static_cast<AuxRecord*>(r);
      uint64_t aux_size = aux->data->aux_size;
      if (aux_size > 0) {
        if (aux_data_buffer_.size() < aux_size) {
          aux_data_buffer_.resize(aux_size);
        }
        if (!record_file_reader_->ReadAuxData(aux->Cpu(), aux->data->aux_offset,
                                              aux_data_buffer_.data(), aux_size)) {
          LOG(ERROR) << "failed to read aux data";
          return false;
        }
        return etm_decoder_->ProcessData(aux_data_buffer_.data(), aux_size, !aux->Unformatted(),
                                         aux->Cpu());
      }
    } else if (r->type() == PERF_RECORD_MMAP && r->InKernel()) {
      auto& mmap_r = *static_cast<MmapRecord*>(r);
      if (android::base::StartsWith(mmap_r.filename, DEFAULT_KERNEL_MMAP_NAME)) {
        kernel_map_start_addr_ = mmap_r.data->addr;
      }
    }
    return true;
  }

  std::unordered_map<Dso*, bool> dso_filter_cache;
  bool FilterDso(Dso* dso) {
    auto lookup = dso_filter_cache.find(dso);
    if (lookup != dso_filter_cache.end()) {
      return lookup->second;
    }
    bool match = std::regex_search(dso->Path(), binary_name_regex_);
    dso_filter_cache.insert({dso, match});
    return match;
  }

  void ProcessInstrRange(const ETMInstrRange& instr_range) {
    if (!FilterDso(instr_range.dso)) {
      return;
    }

    auto& binary = autofdo_binary_map_[instr_range.dso];
    binary.range_count_map[AddrPair(instr_range.start_addr, instr_range.end_addr)] +=
        instr_range.branch_taken_count + instr_range.branch_not_taken_count;
    if (instr_range.branch_taken_count > 0) {
      binary.branch_count_map[AddrPair(instr_range.end_addr, instr_range.branch_to_addr)] +=
          instr_range.branch_taken_count;
    }
  }

  void ProcessBranchList(const ETMBranchList& branch_list) {
    if (!FilterDso(branch_list.dso)) {
      return;
    }

    auto& branch_map = branch_list_binary_map_[branch_list.dso].branch_map;
    ++branch_map[branch_list.addr][branch_list.branch];
  }

  bool ProcessBranchListFile(const std::string& input_filename) {
    if (output_format_ != OutputFormat::AutoFDO) {
      LOG(ERROR) << "Only support autofdo output when given a branch list file.";
      return false;
    }
    // 1. Load EtmBranchList msg from proto file.
    auto fd = FileHelper::OpenReadOnly(input_filename);
    if (!fd.ok()) {
      PLOG(ERROR) << "failed to open " << input_filename;
      return false;
    }
    proto::ETMBranchList branch_list_proto;
    if (!branch_list_proto.ParseFromFileDescriptor(fd)) {
      PLOG(ERROR) << "failed to read msg from " << input_filename;
      return false;
    }
    if (branch_list_proto.magic() != ETM_BRANCH_LIST_PROTO_MAGIC) {
      PLOG(ERROR) << "file not in format etm_branch_list.proto: " << input_filename;
      return false;
    }

    // 2. Build branch map for each binary, convert them to instr ranges.
    auto callback = [this](const ETMInstrRange& range) { ProcessInstrRange(range); };
    auto check_build_id = [](Dso* dso, const BuildId& expected_build_id) {
      if (expected_build_id.IsEmpty()) {
        return true;
      }
      BuildId build_id;
      return GetBuildIdFromDsoPath(dso->GetDebugFilePath(), &build_id) &&
             build_id == expected_build_id;
    };

    for (size_t i = 0; i < branch_list_proto.binaries_size(); i++) {
      const auto& binary_proto = branch_list_proto.binaries(i);
      BuildId build_id(binary_proto.build_id());
      std::optional<DsoType> dso_type = ToDsoType(binary_proto.type());
      if (!dso_type.has_value()) {
        return false;
      }
      std::unique_ptr<Dso> dso =
          Dso::CreateDsoWithBuildId(dso_type.value(), binary_proto.path(), build_id);
      if (!dso || !FilterDso(dso.get()) || !check_build_id(dso.get(), build_id)) {
        continue;
      }
      // Dso is used in ETMInstrRange in post process, so need to extend its lifetime.
      Dso* dso_p = dso.get();
      branch_list_dso_v_.emplace_back(dso.release());
      auto branch_map = BuildBranchMap(binary_proto);

      if (dso_p->type() == DSO_KERNEL) {
        if (!ModifyBranchMapForKernel(binary_proto, dso_p, branch_map)) {
          return false;
        }
      }

      if (auto result = ConvertBranchMapToInstrRanges(dso_p, branch_map, callback); !result.ok()) {
        LOG(WARNING) << "failed to build instr ranges for binary " << dso_p->Path() << ": "
                     << result.error();
      }
    }
    return true;
  }

  BranchMap BuildBranchMap(const proto::ETMBranchList_Binary& binary_proto) {
    BranchMap branch_map;
    for (size_t i = 0; i < binary_proto.addrs_size(); i++) {
      const auto& addr_proto = binary_proto.addrs(i);
      auto& b_map = branch_map[addr_proto.addr()];
      for (size_t j = 0; j < addr_proto.branches_size(); j++) {
        const auto& branch_proto = addr_proto.branches(j);
        std::vector<bool> branch =
            ProtoStringToBranch(branch_proto.branch(), branch_proto.branch_size());
        b_map[branch] = branch_proto.count();
      }
    }
    return branch_map;
  }

  bool ModifyBranchMapForKernel(const proto::ETMBranchList_Binary& binary_proto, Dso* dso,
                                BranchMap& branch_map) {
    if (!binary_proto.has_kernel_info()) {
      LOG(ERROR) << "no kernel info";
      return false;
    }
    uint64_t kernel_map_start_addr = binary_proto.kernel_info().kernel_start_addr();
    if (kernel_map_start_addr == 0) {
      return true;
    }
    // Addresses are still kernel ip addrs in memory. Need to convert them to vaddrs in vmlinux.
    BranchMap new_branch_map;
    for (auto& p : branch_map) {
      uint64_t vaddr_in_file = dso->IpToVaddrInFile(p.first, kernel_map_start_addr, 0);
      new_branch_map[vaddr_in_file] = std::move(p.second);
    }
    branch_map = std::move(new_branch_map);
    return true;
  }

  bool WriteOutput() {
    if (output_format_ == OutputFormat::AutoFDO) {
      return autofdo_writer_.Write(output_filename_);
    }

    CHECK(output_format_ == OutputFormat::BranchList);
    return branch_list_writer_.Write(output_filename_);
  }

  uint64_t GetFirstLoadSegmentVaddr(Dso* dso) {
    ElfStatus status;
    if (auto elf = ElfFile::Open(dso->GetDebugFilePath(), &status); elf) {
      for (const auto& segment : elf->GetProgramHeader()) {
        if (segment.is_load) {
          return segment.vaddr;
        }
      }
    }
    return 0;
  }

  std::optional<DsoType> ToDsoType(proto::ETMBranchList_Binary::BinaryType binary_type) {
    switch (binary_type) {
      case proto::ETMBranchList_Binary::ELF_FILE:
        return DSO_ELF_FILE;
      case proto::ETMBranchList_Binary::KERNEL:
        return DSO_KERNEL;
      case proto::ETMBranchList_Binary::KERNEL_MODULE:
        return DSO_KERNEL_MODULE;
      default:
        LOG(ERROR) << "unexpected binary type " << binary_type;
        return std::nullopt;
    }
  }

  std::regex binary_name_regex_{""};  // Default to match everything.
  bool exclude_perf_ = false;
  std::vector<std::string> input_filenames_;
  std::string output_filename_ = "perf_inject.data";
  OutputFormat output_format_ = OutputFormat::AutoFDO;
  ThreadTreeWithFilter thread_tree_;
  std::unique_ptr<RecordFileReader> record_file_reader_;
  ETMDumpOption etm_dump_option_;
  std::unique_ptr<ETMDecoder> etm_decoder_;
  std::vector<uint8_t> aux_data_buffer_;

  // Store results for AutoFDO.
  std::unordered_map<Dso*, AutoFDOBinaryInfo> autofdo_binary_map_;
  AutoFDOWriter autofdo_writer_;
  // Store results for BranchList.
  std::unordered_map<Dso*, BranchListBinaryInfo> branch_list_binary_map_;
  BranchListWriter branch_list_writer_;
  std::vector<std::unique_ptr<Dso>> branch_list_dso_v_;
  uint64_t kernel_map_start_addr_ = 0;
};

}  // namespace

void RegisterInjectCommand() {
  return RegisterCommand("inject", [] { return std::unique_ptr<Command>(new InjectCommand); });
}

}  // namespace simpleperf
