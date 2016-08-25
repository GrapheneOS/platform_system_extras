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

#include "dso.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <limits>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>

#include "environment.h"
#include "read_apk.h"
#include "read_elf.h"
#include "utils.h"

static OneTimeFreeAllocator symbol_name_allocator;

Symbol::Symbol(const std::string& name, uint64_t addr, uint64_t len)
    : addr(addr),
      len(len),
      name_(symbol_name_allocator.AllocateString(name)),
      demangled_name_(nullptr),
      has_dumped_(false) {}

const char* Symbol::DemangledName() const {
  if (demangled_name_ == nullptr) {
    const std::string s = Dso::Demangle(name_);
    if (s == name_) {
      demangled_name_ = name_;
    } else {
      demangled_name_ = symbol_name_allocator.AllocateString(s);
    }
  }
  return demangled_name_;
}

bool Dso::demangle_ = true;
std::string Dso::symfs_dir_;
std::string Dso::vmlinux_;
std::string Dso::kallsyms_;
std::unordered_map<std::string, BuildId> Dso::build_id_map_;
size_t Dso::dso_count_;

void Dso::SetDemangle(bool demangle) { demangle_ = demangle; }

extern "C" char* __cxa_demangle(const char* mangled_name, char* buf, size_t* n,
                                int* status);

std::string Dso::Demangle(const std::string& name) {
  if (!demangle_) {
    return name;
  }
  int status;
  bool is_linker_symbol = (name.find(linker_prefix) == 0);
  const char* mangled_str = name.c_str();
  if (is_linker_symbol) {
    mangled_str += linker_prefix.size();
  }
  std::string result = name;
  char* demangled_name = __cxa_demangle(mangled_str, nullptr, nullptr, &status);
  if (status == 0) {
    if (is_linker_symbol) {
      result = std::string("[linker]") + demangled_name;
    } else {
      result = demangled_name;
    }
    free(demangled_name);
  } else if (is_linker_symbol) {
    result = std::string("[linker]") + mangled_str;
  }
  return result;
}

bool Dso::SetSymFsDir(const std::string& symfs_dir) {
  std::string dirname = symfs_dir;
  if (!dirname.empty()) {
    if (dirname.back() != '/') {
      dirname.push_back('/');
    }
    if (GetEntriesInDir(symfs_dir).empty()) {
      LOG(ERROR) << "Invalid symfs_dir '" << symfs_dir << "'";
      return false;
    }
  }
  symfs_dir_ = dirname;
  return true;
}

void Dso::SetVmlinux(const std::string& vmlinux) { vmlinux_ = vmlinux; }

void Dso::SetBuildIds(
    const std::vector<std::pair<std::string, BuildId>>& build_ids) {
  std::unordered_map<std::string, BuildId> map;
  for (auto& pair : build_ids) {
    LOG(DEBUG) << "build_id_map: " << pair.first << ", "
               << pair.second.ToString();
    map.insert(pair);
  }
  build_id_map_ = std::move(map);
}

BuildId Dso::GetExpectedBuildId() {
  auto it = build_id_map_.find(path_);
  if (it != build_id_map_.end()) {
    return it->second;
  }
  return BuildId();
}

std::unique_ptr<Dso> Dso::CreateDso(DsoType dso_type,
                                    const std::string& dso_path) {
  static uint64_t id = 0;
  return std::unique_ptr<Dso>(new Dso(dso_type, ++id, dso_path));
}

Dso::Dso(DsoType type, uint64_t id, const std::string& path)
    : type_(type),
      id_(id),
      path_(path),
      debug_file_path_(path),
      min_vaddr_(std::numeric_limits<uint64_t>::max()),
      is_loaded_(false),
      has_dumped_(false) {
  // Check if file matching path_ exists in symfs directory before using it as
  // debug_file_path_.
  if (!symfs_dir_.empty()) {
    std::string path_in_symfs = symfs_dir_ + path_;
    std::tuple<bool, std::string, std::string> tuple =
        SplitUrlInApk(path_in_symfs);
    std::string file_path =
        std::get<0>(tuple) ? std::get<1>(tuple) : path_in_symfs;
    if (IsRegularFile(file_path)) {
      debug_file_path_ = path_in_symfs;
    }
  }
  size_t pos = path.find_last_of("/\\");
  if (pos != std::string::npos) {
    file_name_ = path.substr(pos + 1);
  } else {
    file_name_ = path;
  }
  dso_count_++;
}

Dso::~Dso() {
  if (--dso_count_ == 0) {
    // Clean up global variables when no longer used.
    symbol_name_allocator.Clear();
    demangle_ = true;
    symfs_dir_.clear();
    vmlinux_.clear();
    kallsyms_.clear();
    build_id_map_.clear();
  }
}

const Symbol* Dso::FindSymbol(uint64_t vaddr_in_dso) {
  if (!is_loaded_) {
    is_loaded_ = true;
    // If symbols has been read from SymbolRecords, no need to load them from
    // dso.
    if (symbols_.empty()) {
      if (!Load()) {
        LOG(DEBUG) << "failed to load dso: " << path_;
        return nullptr;
      }
    }
  }
  if (symbols_.empty()) {
    return nullptr;
  }

  auto it = symbols_.upper_bound(Symbol("", vaddr_in_dso, 0));
  if (it != symbols_.begin()) {
    --it;
    if (it->addr <= vaddr_in_dso && it->addr + it->len > vaddr_in_dso) {
      return &*it;
    }
  }
  return nullptr;
}

uint64_t Dso::MinVirtualAddress() {
  if (min_vaddr_ == std::numeric_limits<uint64_t>::max()) {
    min_vaddr_ = 0;
    if (type_ == DSO_ELF_FILE) {
      BuildId build_id = GetExpectedBuildId();

      uint64_t addr;
      ElfStatus result = ReadMinExecutableVirtualAddressFromElfFile(
          GetDebugFilePath(), build_id, &addr);
      if (result != ElfStatus::NO_ERROR) {
        LOG(WARNING) << "failed to read min virtual address of "
                     << GetDebugFilePath() << ": " << result;
      } else {
        min_vaddr_ = addr;
      }
    }
  }
  return min_vaddr_;
}

bool Dso::Load() {
  bool result = false;
  switch (type_) {
    case DSO_KERNEL:
      result = LoadKernel();
      break;
    case DSO_KERNEL_MODULE:
      result = LoadKernelModule();
      break;
    case DSO_ELF_FILE: {
      if (std::get<0>(SplitUrlInApk(path_))) {
        result = LoadEmbeddedElfFile();
      } else {
        result = LoadElfFile();
      }
      break;
    }
  }
  if (result) {
    FixupSymbolLength();
  } else {
    symbols_.clear();
  }
  return result;
}

static bool IsKernelFunctionSymbol(const KernelSymbol& symbol) {
  return (symbol.type == 'T' || symbol.type == 't' || symbol.type == 'W' ||
          symbol.type == 'w');
}

static bool KernelSymbolCallback(const KernelSymbol& kernel_symbol, Dso* dso) {
  if (IsKernelFunctionSymbol(kernel_symbol)) {
    dso->InsertSymbol(Symbol(kernel_symbol.name, kernel_symbol.addr, 0));
  }
  return false;
}

static void VmlinuxSymbolCallback(const ElfFileSymbol& elf_symbol, Dso* dso) {
  if (elf_symbol.is_func) {
    dso->InsertSymbol(
        Symbol(elf_symbol.name, elf_symbol.vaddr, elf_symbol.len));
  }
}

bool CheckReadSymbolResult(ElfStatus result, const std::string& filename) {
  if (result == ElfStatus::NO_ERROR) {
    LOG(VERBOSE) << "Read symbols from " << filename << " successfully";
    return true;
  } else if (result == ElfStatus::NO_SYMBOL_TABLE) {
    // Lacking symbol table isn't considered as an error but worth reporting.
    LOG(WARNING) << filename << " doesn't contain symbol table";
    return true;
  } else {
    LOG(WARNING) << "failed to read symbols from " << filename
                 << ": " << result;
    return false;
  }
}

bool Dso::LoadKernel() {
  BuildId build_id = GetExpectedBuildId();
  if (!vmlinux_.empty()) {
    ElfStatus result = ParseSymbolsFromElfFile(vmlinux_, build_id,
        std::bind(VmlinuxSymbolCallback, std::placeholders::_1, this));
    return CheckReadSymbolResult(result, vmlinux_);
  } else if (!kallsyms_.empty()) {
    ProcessKernelSymbols(kallsyms_, std::bind(&KernelSymbolCallback,
                                              std::placeholders::_1, this));
    bool all_zero = true;
    for (const auto& symbol : symbols_) {
      if (symbol.addr != 0) {
        all_zero = false;
        break;
      }
    }
    if (all_zero) {
      LOG(WARNING)
          << "Symbol addresses in /proc/kallsyms on device are all zero. "
             "`echo 0 >/proc/sys/kernel/kptr_restrict` if possible.";
      symbols_.clear();
      return false;
    }
  } else {
    if (!build_id.IsEmpty()) {
      BuildId real_build_id;
      if (!GetKernelBuildId(&real_build_id)) {
        return false;
      }
      bool match = (build_id == real_build_id);
      if (!match) {
        LOG(WARNING) << "failed to read symbols from /proc/kallsyms: Build id "
                     << "mismatch";
        return false;
      }
    }

    std::string kallsyms;
    if (!android::base::ReadFileToString("/proc/kallsyms", &kallsyms)) {
      LOG(DEBUG) << "failed to read /proc/kallsyms";
      return false;
    }
    ProcessKernelSymbols(kallsyms, std::bind(&KernelSymbolCallback,
                                             std::placeholders::_1, this));
    bool all_zero = true;
    for (const auto& symbol : symbols_) {
      if (symbol.addr != 0) {
        all_zero = false;
        break;
      }
    }
    if (all_zero) {
      LOG(WARNING) << "Symbol addresses in /proc/kallsyms are all zero. "
                      "`echo 0 >/proc/sys/kernel/kptr_restrict` if possible.";
      symbols_.clear();
      return false;
    }
  }
  return true;
}

static void ElfFileSymbolCallback(const ElfFileSymbol& elf_symbol, Dso* dso,
                                  bool (*filter)(const ElfFileSymbol&)) {
  if (filter(elf_symbol)) {
    dso->InsertSymbol(
        Symbol(elf_symbol.name, elf_symbol.vaddr, elf_symbol.len));
  }
}

static bool SymbolFilterForKernelModule(const ElfFileSymbol& elf_symbol) {
  // TODO: Parse symbol outside of .text section.
  return (elf_symbol.is_func && elf_symbol.is_in_text_section);
}

bool Dso::LoadKernelModule() {
  BuildId build_id = GetExpectedBuildId();
  ElfStatus result = ParseSymbolsFromElfFile(GetDebugFilePath(), build_id,
      std::bind(ElfFileSymbolCallback, std::placeholders::_1, this,
                SymbolFilterForKernelModule));
  return CheckReadSymbolResult(result, GetDebugFilePath());
}

static bool SymbolFilterForDso(const ElfFileSymbol& elf_symbol) {
  return elf_symbol.is_func ||
         (elf_symbol.is_label && elf_symbol.is_in_text_section);
}

bool Dso::LoadElfFile() {
  BuildId build_id = GetExpectedBuildId();

  if (symfs_dir_.empty()) {
    // Linux host can store debug shared libraries in /usr/lib/debug.
    ElfStatus result = ParseSymbolsFromElfFile(
        "/usr/lib/debug" + path_, build_id,
        std::bind(ElfFileSymbolCallback, std::placeholders::_1, this,
                  SymbolFilterForDso));
    if (result == ElfStatus::NO_ERROR) {
      return CheckReadSymbolResult(result, "/usr/lib/debug" + path_);
    }
  }
  ElfStatus result = ParseSymbolsFromElfFile(
      GetDebugFilePath(), build_id,
      std::bind(ElfFileSymbolCallback, std::placeholders::_1, this,
                SymbolFilterForDso));
  return CheckReadSymbolResult(result, GetDebugFilePath());
}

bool Dso::LoadEmbeddedElfFile() {
  BuildId build_id = GetExpectedBuildId();
  auto tuple = SplitUrlInApk(GetDebugFilePath());
  CHECK(std::get<0>(tuple));
  ElfStatus result = ParseSymbolsFromApkFile(
      std::get<1>(tuple), std::get<2>(tuple), build_id,
      std::bind(ElfFileSymbolCallback, std::placeholders::_1, this,
                SymbolFilterForDso));
  return CheckReadSymbolResult(result, GetDebugFilePath());
}

void Dso::InsertSymbol(const Symbol& symbol) { symbols_.insert(symbol); }

void Dso::FixupSymbolLength() {
  Symbol* prev_symbol = nullptr;
  for (auto& symbol : symbols_) {
    if (prev_symbol != nullptr && prev_symbol->len == 0) {
      prev_symbol->len = symbol.addr - prev_symbol->addr;
    }
    prev_symbol = const_cast<Symbol*>(&symbol);
  }
  if (prev_symbol != nullptr && prev_symbol->len == 0) {
    prev_symbol->len = std::numeric_limits<uint64_t>::max() - prev_symbol->addr;
  }
}

const char* DsoTypeToString(DsoType dso_type) {
  switch (dso_type) {
    case DSO_KERNEL:
      return "dso_kernel";
    case DSO_KERNEL_MODULE:
      return "dso_kernel_module";
    case DSO_ELF_FILE:
      return "dso_elf_file";
    default:
      return "unknown";
  }
}
