/*
 * Copyright (C) 2020 The Android Open Source Project
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
#include "kallsyms.h"

#include <inttypes.h>

#include <string>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>

#include "environment.h"
#include "utils.h"

namespace simpleperf {

namespace {

const char kKallsymsPath[] = "/proc/kallsyms";
const char kProcModulesPath[] = "/proc/modules";
const char kPtrRestrictPath[] = "/proc/sys/kernel/kptr_restrict";
const char kLowerPtrRestrictAndroidProp[] = "security.lower_kptr_restrict";
const unsigned int kMinLineTestNonNullSymbols = 10;

// Tries to read the kernel symbol file and ensure that at least some symbol
// addresses are non-null.
bool CanReadKernelSymbolAddresses() {
  FILE* fp = fopen(kKallsymsPath, "re");
  if (fp == nullptr) {
    LOG(DEBUG) << "Failed to read " << kKallsymsPath;
    return false;
  }
  LineReader reader(fp);
  auto symbol_callback = [&](const KernelSymbol& symbol) { return (symbol.addr != 0u); };
  for (unsigned int i = 0; i < kMinLineTestNonNullSymbols; i++) {
    char* line = reader.ReadLine();
    if (line == nullptr) {
      return false;
    }
    std::string l = std::string(line);
    if (ProcessKernelSymbols(l, symbol_callback)) {
      return true;
    }
  }
  return false;
}

// Define a scope in which access to kallsyms is possible.
// This is based on the Perfetto implementation.
class ScopedKptrUnrestrict {
 public:
  ScopedKptrUnrestrict(bool use_property = false);  // Lowers kptr_restrict if necessary.
  ~ScopedKptrUnrestrict();                          // Restores the initial kptr_restrict.

  bool KallsymsAvailable();  // Indicates if access to kallsyms should be successful.

 private:
  static bool WriteKptrRestrict(const std::string&);

  std::string initial_value_;
  bool use_property_;
  bool restore_on_dtor_ = true;
  bool kallsyms_available_ = false;
};

ScopedKptrUnrestrict::ScopedKptrUnrestrict(bool use_property) : use_property_(use_property) {
  if (CanReadKernelSymbolAddresses()) {
    // Everything seems to work (e.g., we are running as root and kptr_restrict
    // is < 2). Don't touching anything.
    restore_on_dtor_ = false;
    kallsyms_available_ = true;
    return;
  }

  if (use_property_) {
    bool ret = android::base::SetProperty(kLowerPtrRestrictAndroidProp, "1");
    if (!ret) {
      LOG(ERROR) << "Unable to set " << kLowerPtrRestrictAndroidProp << " to 1.";
      return;
    }
    // Init takes some time to react to the property change.
    // Unfortunately, we cannot read kptr_restrict because of SELinux. Instead,
    // we detect this by reading the initial lines of kallsyms and checking
    // that they are non-zero. This loop waits for at most 250ms (50 * 5ms).
    for (int attempt = 1; attempt <= 50; ++attempt) {
      usleep(5000);
      if (CanReadKernelSymbolAddresses()) {
        kallsyms_available_ = true;
        return;
      }
    }
    LOG(ERROR) << "kallsyms addresses are still masked after setting "
               << kLowerPtrRestrictAndroidProp;
    return;
  }

  // Otherwise, read the kptr_restrict value and lower it if needed.
  bool read_res = android::base::ReadFileToString(kPtrRestrictPath, &initial_value_);
  if (!read_res) {
    LOG(WARNING) << "Failed to read " << kPtrRestrictPath;
    return;
  }

  // Progressively lower kptr_restrict until we can read kallsyms.
  for (int value = atoi(initial_value_.c_str()); value > 0; --value) {
    bool ret = WriteKptrRestrict(std::to_string(value));
    if (!ret) {
      LOG(WARNING) << "Access to kernel symbol addresses is restricted. If "
                   << "possible, please do `echo 0 >/proc/sys/kernel/kptr_restrict` "
                   << "to fix this.";
      return;
    }
    if (CanReadKernelSymbolAddresses()) {
      kallsyms_available_ = true;
      return;
    }
  }
}

ScopedKptrUnrestrict::~ScopedKptrUnrestrict() {
  if (!restore_on_dtor_) return;
  if (use_property_) {
    android::base::SetProperty(kLowerPtrRestrictAndroidProp, "0");
  } else if (!initial_value_.empty()) {
    WriteKptrRestrict(initial_value_);
  }
}

bool ScopedKptrUnrestrict::KallsymsAvailable() {
  return kallsyms_available_;
}

bool ScopedKptrUnrestrict::WriteKptrRestrict(const std::string& value) {
  if (!android::base::WriteStringToFile(value, kPtrRestrictPath)) {
    LOG(WARNING) << "Failed to set " << kPtrRestrictPath << " to " << value;
    return false;
  }
  return true;
}

}  // namespace

bool ProcessKernelSymbols(std::string& symbol_data,
                          const std::function<bool(const KernelSymbol&)>& callback) {
  char* p = &symbol_data[0];
  char* data_end = p + symbol_data.size();
  while (p < data_end) {
    char* line_end = strchr(p, '\n');
    if (line_end != nullptr) {
      *line_end = '\0';
    }
    size_t line_size = (line_end != nullptr) ? (line_end - p) : (data_end - p);
    // Parse line like: ffffffffa005c4e4 d __warned.41698       [libsas]
    char name[line_size];
    char module[line_size];
    strcpy(module, "");

    KernelSymbol symbol;
    int ret = sscanf(p, "%" PRIx64 " %c %s%s", &symbol.addr, &symbol.type, name, module);
    if (line_end != nullptr) {
      *line_end = '\n';
      p = line_end + 1;
    } else {
      p = data_end;
    }
    if (ret >= 3) {
      symbol.name = name;
      size_t module_len = strlen(module);
      if (module_len > 2 && module[0] == '[' && module[module_len - 1] == ']') {
        module[module_len - 1] = '\0';
        symbol.module = &module[1];
      } else {
        symbol.module = nullptr;
      }

      if (callback(symbol)) {
        return true;
      }
    }
  }
  return false;
}

std::vector<KernelMmap> GetLoadedModules() {
  ScopedKptrUnrestrict kptr_unrestrict;
  if (!kptr_unrestrict.KallsymsAvailable()) return {};
  std::vector<KernelMmap> result;
  FILE* fp = fopen(kProcModulesPath, "re");
  if (fp == nullptr) {
    // There is no /proc/modules on Android devices, so we don't print error if failed to open it.
    PLOG(DEBUG) << "failed to open file /proc/modules";
    return result;
  }
  LineReader reader(fp);
  char* line;
  while ((line = reader.ReadLine()) != nullptr) {
    // Parse line like: nf_defrag_ipv6 34768 1 nf_conntrack_ipv6, Live 0xffffffffa0fe5000
    char name[reader.MaxLineSize()];
    uint64_t addr;
    uint64_t len;
    if (sscanf(line, "%s%" PRIu64 "%*u%*s%*s 0x%" PRIx64, name, &len, &addr) == 3) {
      KernelMmap map;
      map.name = name;
      map.start_addr = addr;
      map.len = len;
      result.push_back(map);
    }
  }
  bool all_zero = true;
  for (const auto& map : result) {
    if (map.start_addr != 0) {
      all_zero = false;
    }
  }
  if (all_zero) {
    LOG(DEBUG) << "addresses in /proc/modules are all zero, so ignore kernel modules";
    return std::vector<KernelMmap>();
  }
  return result;
}

uint64_t GetKernelStartAddress() {
  ScopedKptrUnrestrict kptr_unrestrict;
  if (!kptr_unrestrict.KallsymsAvailable()) return 0;
  FILE* fp = fopen(kKallsymsPath, "re");
  if (fp == nullptr) {
    return 0;
  }
  LineReader reader(fp);
  char* line;
  while ((line = reader.ReadLine()) != nullptr) {
    if (strstr(line, "_stext") != nullptr) {
      uint64_t addr;
      if (sscanf(line, "%" PRIx64, &addr) == 1) {
        return addr;
      }
    }
  }
  return 0;
}

bool LoadKernelSymbols(std::string* kallsyms, bool use_property /* = false */) {
  ScopedKptrUnrestrict kptr_unrestrict(use_property);
  if (kptr_unrestrict.KallsymsAvailable()) {
    return android::base::ReadFileToString(kKallsymsPath, kallsyms);
  }
  return false;
}

}  // namespace simpleperf
