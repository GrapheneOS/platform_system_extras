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

#ifndef SIMPLE_PERF_DSO_H_
#define SIMPLE_PERF_DSO_H_

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "build_id.h"

struct Symbol {
  uint64_t addr;
  uint64_t len;

  Symbol(const std::string& name, uint64_t addr, uint64_t len);
  const char* Name() const { return name_; }

  const char* DemangledName() const;

  bool HasDumped() const { return has_dumped_; }

  void SetDumped() const { has_dumped_ = true; }

 private:
  const char* name_;
  mutable const char* demangled_name_;
  mutable bool has_dumped_;
};

struct SymbolComparator {
  bool operator()(const Symbol& symbol1, const Symbol& symbol2) {
    return symbol1.addr < symbol2.addr;
  }
};

enum DsoType {
  DSO_KERNEL,
  DSO_KERNEL_MODULE,
  DSO_ELF_FILE,
};

struct KernelSymbol;
struct ElfFileSymbol;

struct Dso {
 public:
  static void SetDemangle(bool demangle);
  static std::string Demangle(const std::string& name);
  static bool SetSymFsDir(const std::string& symfs_dir);
  static void SetVmlinux(const std::string& vmlinux);
  static void SetKallsyms(std::string kallsyms) {
    if (!kallsyms.empty()) {
      kallsyms_ = std::move(kallsyms);
    }
  }
  static void SetBuildIds(
      const std::vector<std::pair<std::string, BuildId>>& build_ids);

  static std::unique_ptr<Dso> CreateDso(DsoType dso_type,
                                        const std::string& dso_path);

  ~Dso();

  DsoType type() const { return type_; }

  uint64_t id() const { return id_; }

  // Return the path recorded in perf.data.
  const std::string& Path() const { return path_; }

  bool HasDumped() const { return has_dumped_; }

  void SetDumped() { has_dumped_ = true; }

  // Return the accessible path. It may be the same as Path(), or
  // return the path with prefix set by SetSymFsDir().
  std::string GetAccessiblePath() const;

  // Return the minimum virtual address in program header.
  uint64_t MinVirtualAddress();

  const Symbol* FindSymbol(uint64_t vaddr_in_dso);
  void InsertSymbol(const Symbol& symbol);

 private:
  static BuildId GetExpectedBuildId(const std::string& filename);
  static bool KernelSymbolCallback(const KernelSymbol& kernel_symbol, Dso* dso);
  static void VmlinuxSymbolCallback(const ElfFileSymbol& elf_symbol, Dso* dso);
  static void ElfFileSymbolCallback(const ElfFileSymbol& elf_symbol, Dso* dso,
                                    bool (*filter)(const ElfFileSymbol&));

  static bool demangle_;
  static std::string symfs_dir_;
  static std::string vmlinux_;
  static std::string kallsyms_;
  static std::unordered_map<std::string, BuildId> build_id_map_;
  static size_t dso_count_;

  Dso(DsoType type, uint64_t id, const std::string& path);
  bool Load();
  bool LoadKernel();
  bool LoadKernelModule();
  bool LoadElfFile();
  bool LoadEmbeddedElfFile();
  void FixupSymbolLength();

  const DsoType type_;
  const uint64_t id_;
  const std::string path_;
  uint64_t min_vaddr_;
  std::set<Symbol, SymbolComparator> symbols_;
  bool is_loaded_;
  bool has_dumped_;
};

const char* DsoTypeToString(DsoType dso_type);

#endif  // SIMPLE_PERF_DSO_H_
