
#include "perf_data_converter.h"

#include <algorithm>
#include <limits>
#include <map>
#include <unordered_map>

#include <android-base/strings.h>

#include "perf_profile.pb.h"

#include "quipper/perf_parser.h"
#include "symbolizer.h"

using std::map;

namespace wireless_android_logging_awp {

// Flag to turn off symbolization, even if a symbolizer is given.
static constexpr bool kUseSymbolizer = true;

// If this flag is true, symbols will be computed on-device for all samples. If this
// flag is false, this will only be done for modules without a build id (i.e., where
// symbols cannot be derived in the cloud).
//
// This is turned off for now to conserve space.
static constexpr bool kUseSymbolizerForModulesWithBuildId = false;

typedef quipper::ParsedEvent::DSOAndOffset DSOAndOffset;
typedef std::vector<DSOAndOffset> callchain;

struct callchain_lt {
  bool operator()(const callchain *c1, const callchain *c2) const {
    if (c1->size() != c2->size()) {
      return c1->size() < c2->size();
    }
    for (unsigned idx = 0; idx < c1->size(); ++idx) {
      const DSOAndOffset *do1 = &(*c1)[idx];
      const DSOAndOffset *do2 = &(*c2)[idx];
      if (do1->offset() != do2->offset()) {
        return do1->offset() < do2->offset();
      }
      int rc = do1->dso_name().compare(do2->dso_name());
      if (rc) {
        return rc < 0;
      }
    }
    return false;
  }
};

struct RangeTarget {
  RangeTarget(uint64 start, uint64 end, uint64 to)
      : start(start), end(end), to(to) {}

  bool operator<(const RangeTarget &r) const {
    if (start != r.start) {
      return start < r.start;
    } else if (end != r.end) {
      return end < r.end;
    } else {
      return to < r.to;
    }
  }
  uint64 start;
  uint64 end;
  uint64 to;
};

struct BinaryProfile {
  map<uint64, uint64> address_count_map;
  map<RangeTarget, uint64> range_count_map;
  map<const callchain *, uint64, callchain_lt> callchain_count_map;
};

wireless_android_play_playlog::AndroidPerfProfile*
RawPerfDataToAndroidPerfProfile(const string &perf_file,
                                ::perfprofd::Symbolizer* symbolizer) {
  quipper::PerfParser parser;
  if (!parser.ReadFile(perf_file) || !parser.ParseRawEvents()) {
    return nullptr;
  }
  std::unique_ptr<wireless_android_play_playlog::AndroidPerfProfile> ret(
      new wireless_android_play_playlog::AndroidPerfProfile());

  using ModuleProfileMap = std::map<string, BinaryProfile>;
  using Program = std::pair<uint32_t /* index into process name table, or uint32_t max */,
                            std::string /* program name = comm of thread */>;
  using ProgramProfileMap = std::map<Program, ModuleProfileMap>;

  struct ProcessNameTable {
    std::vector<std::string> names;
    std::unordered_map<std::string, uint32_t> index_lookup;
  };
  constexpr uint32_t kNoProcessNameTableEntry = std::numeric_limits<uint32_t>::max();
  ProcessNameTable process_name_table;

  // Note: the callchain_count_map member in BinaryProfile contains
  // pointers into callchains owned by "parser" above, meaning
  // that once the parser is destroyed, callchain pointers in
  // name_profile_map will become stale (e.g. keep these two
  // together in the same region).
  ProgramProfileMap name_profile_map;
  uint64 total_samples = 0;
  bool seen_branch_stack = false;
  bool seen_callchain = false;

  auto is_kernel_dso = [](const std::string& dso) {
    constexpr const char* kKernelDsos[] = {
        "[kernel.kallsyms]",
        "[vdso]",
    };
    for (auto kernel_dso : kKernelDsos) {
      if (dso == kernel_dso) {
        return true;
      }
    }
    return false;
  };

  for (const auto &event : parser.parsed_events()) {
    if (!event.raw_event ||
        event.raw_event->header.type != PERF_RECORD_SAMPLE) {
      continue;
    }
    string dso_name = event.dso_and_offset.dso_name();
    Program program_id;
    {
      std::string program_name = event.command();
      const std::string kernel_name = "[kernel.kallsyms]";
      if (android::base::StartsWith(dso_name, kernel_name)) {
        dso_name = kernel_name;
        if (program_name == "") {
          program_name = "kernel";
        }
      } else if (program_name == "") {
        if (is_kernel_dso(dso_name)) {
          program_name = "kernel";
        } else {
          program_name = "unknown_program";
        }
      }
      std::string process_name = event.process_command();
      uint32_t process_name_index = kNoProcessNameTableEntry;
      if (!process_name.empty()) {
        auto name_iter = process_name_table.index_lookup.find(process_name);
        if (name_iter == process_name_table.index_lookup.end()) {
          process_name_index = process_name_table.names.size();
          process_name_table.index_lookup.emplace(process_name, process_name_index);
          process_name_table.names.push_back(process_name);
        } else {
          process_name_index = name_iter->second;
        }
      }
      program_id = std::make_pair(process_name_index, program_name);
    }
    ModuleProfileMap& module_profile_map = name_profile_map[program_id];

    total_samples++;
    // We expect to see either all callchain events, all branch stack
    // events, or all flat sample events, not a mix. For callchains,
    // however, it can be the case that none of the IPs in a chain
    // are mappable, in which case the parsed/mapped chain will appear
    // empty (appearing as a flat sample).
    if (!event.callchain.empty()) {
      CHECK(!seen_branch_stack && "examining callchain");
      seen_callchain = true;
      const callchain *cc = &event.callchain;
      module_profile_map[dso_name].callchain_count_map[cc]++;
    } else if (!event.branch_stack.empty()) {
      CHECK(!seen_callchain && "examining branch stack");
      seen_branch_stack = true;
      module_profile_map[dso_name].address_count_map[
          event.dso_and_offset.offset()]++;
    } else {
      module_profile_map[dso_name].address_count_map[
          event.dso_and_offset.offset()]++;
    }
    for (size_t i = 1; i < event.branch_stack.size(); i++) {
      if (dso_name == event.branch_stack[i - 1].to.dso_name()) {
        uint64 start = event.branch_stack[i].to.offset();
        uint64 end = event.branch_stack[i - 1].from.offset();
        uint64 to = event.branch_stack[i - 1].to.offset();
        // The interval between two taken branches should not be too large.
        if (end < start || end - start > (1 << 20)) {
          LOG(WARNING) << "Bogus LBR data: " << start << "->" << end;
          continue;
        }
        module_profile_map[dso_name].range_count_map[
            RangeTarget(start, end, to)]++;
      }
    }
  }

  struct ModuleData {
    int index = 0;

    std::vector<std::string> symbols;
    std::unordered_map<uint64_t, size_t> addr_to_symbol_index;

    wireless_android_play_playlog::LoadModule* module = nullptr;
  };
  map<string, ModuleData> name_data_map;
  for (const auto &program_profile : name_profile_map) {
    for (const auto &module_profile : program_profile.second) {
      name_data_map[module_profile.first] = ModuleData();
    }
  }
  int current_index = 0;
  for (auto iter = name_data_map.begin(); iter != name_data_map.end(); ++iter) {
    iter->second.index = current_index++;
  }

  map<string, string> name_buildid_map;
  parser.GetFilenamesToBuildIDs(&name_buildid_map);
  ret->set_total_samples(total_samples);

  for (auto& name_data : name_data_map) {
    auto load_module = ret->add_load_modules();
    load_module->set_name(name_data.first);
    auto nbmi = name_buildid_map.find(name_data.first);
    bool has_build_id = nbmi != name_buildid_map.end();
    if (has_build_id) {
      const std::string &build_id = nbmi->second;
      if (build_id.size() == 40 && build_id.substr(32) == "00000000") {
        load_module->set_build_id(build_id.substr(0, 32));
      } else {
        load_module->set_build_id(build_id);
      }
    }
    if (kUseSymbolizer && symbolizer != nullptr && !is_kernel_dso(name_data.first)) {
      if (kUseSymbolizerForModulesWithBuildId || !has_build_id) {
        // Add the module to signal that we'd want to add symbols.
        name_data.second.module = load_module;
      }
    }
  }

  auto symbolize = [symbolizer](ModuleData* module_data,
                                const std::string& dso,
                                uint64_t address) {
    if (module_data->module == nullptr) {
      return address;
    }
    auto it = module_data->addr_to_symbol_index.find(address);
    size_t index = std::numeric_limits<size_t>::max();
    if (it == module_data->addr_to_symbol_index.end()) {
      std::string symbol = symbolizer->Decode(dso, address);
      if (!symbol.empty()) {
        // Deduplicate symbols.
        auto it = std::find(module_data->symbols.begin(), module_data->symbols.end(), symbol);
        if (it == module_data->symbols.end()) {
          index = module_data->symbols.size();
          module_data->symbols.push_back(symbol);
        } else {
          index = it - module_data->symbols.begin();
        }
        module_data->addr_to_symbol_index.emplace(address, index);
      }
    } else {
      index = it->second;
    }
    if (index != std::numeric_limits<size_t>::max()) {
      // Note: consider an actual entry in the proto? Maybe a oneof? But that
          //       will be complicated with the separate repeated addr & module.
      address = std::numeric_limits<size_t>::max() - index;
    }
    return address;
  };

  for (const auto &program_profile : name_profile_map) {
    auto program = ret->add_programs();
    const Program& program_id = program_profile.first;
    program->set_name(program_id.second);
    if (program_id.first != kNoProcessNameTableEntry) {
      program->set_process_name_id(program_id.first);
    }
    for (const auto &module_profile : program_profile.second) {
      ModuleData& module_data = name_data_map[module_profile.first];
      int32 module_id = module_data.index;
      auto module = program->add_modules();
      module->set_load_module_id(module_id);

      // TODO: Templatize to avoid branch overhead?
      for (const auto &addr_count : module_profile.second.address_count_map) {
        auto address_samples = module->add_address_samples();

        uint64_t address = symbolize(&module_data, module_profile.first, addr_count.first);
        address_samples->add_address(address);
        address_samples->set_count(addr_count.second);
      }
      for (const auto &range_count : module_profile.second.range_count_map) {
        auto range_samples = module->add_range_samples();
        range_samples->set_start(range_count.first.start);
        range_samples->set_end(range_count.first.end);
        range_samples->set_to(range_count.first.to);
        range_samples->set_count(range_count.second);
      }
      for (const auto &callchain_count :
               module_profile.second.callchain_count_map) {
        auto address_samples = module->add_address_samples();
        address_samples->set_count(callchain_count.second);
        for (const auto &d_o : *callchain_count.first) {
          ModuleData& module_data = name_data_map[d_o.dso_name()];
          int32 module_id = module_data.index;
          address_samples->add_load_module_id(module_id);
          address_samples->add_address(symbolize(&module_data, d_o.dso_name(), d_o.offset()));
        }
      }
    }
  }

  for (auto& name_data : name_data_map) {
    auto load_module = name_data.second.module;
    if (load_module != nullptr) {
      for (const std::string& symbol : name_data.second.symbols) {
        load_module->add_symbol(symbol);
      }
    }
  }

  if (!process_name_table.names.empty()) {
    wireless_android_play_playlog::ProcessNames* process_names = ret->mutable_process_names();
    for (const std::string& name : process_name_table.names) {
      process_names->add_name(name);
    }
  }

  return ret.release();
}

}  // namespace wireless_android_logging_awp
