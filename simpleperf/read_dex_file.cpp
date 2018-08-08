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

#include "read_dex_file.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <functional>
#include <string>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>

#include <dex/class_accessor-inl.h>
#include <dex/code_item_accessors-inl.h>
#include <dex/dex_file_loader.h>
#include <dex/dex_file.h>

static bool OpenDexFiles(void* addr, uint64_t size, std::vector<uint64_t> dex_file_offsets,
                         const std::function<void (const art::DexFile&, uint64_t)>& callback) {
  bool result = true;
  for (uint64_t offset : dex_file_offsets) {
    if (offset >= size || size - offset < sizeof(art::DexFile::Header)) {
      result = false;
      break;
    }
    auto header = reinterpret_cast<art::DexFile::Header*>(static_cast<char*>(addr) + offset);
    if (size - offset < header->file_size_) {
      result = false;
      break;
    }
    art::DexFileLoader loader;
    std::string error;
    std::unique_ptr<const art::DexFile> dex_file = loader.Open(reinterpret_cast<uint8_t*>(header),
                                                               header->file_size_, "", 0, nullptr,
                                                               false, false, &error);
    if (!dex_file) {
      result = false;
      break;
    }
    callback(*dex_file, offset);
  }
  return result;
}

bool ReadSymbolsFromDexFileInMemory(void* addr, uint64_t size,
                                    const std::vector<uint64_t>& dex_file_offsets,
                                    std::vector<DexFileSymbol>* symbols) {
  auto dexfile_callback = [&](const art::DexFile& dex_file, uint64_t dex_file_offset) {
    for (art::ClassAccessor accessor : dex_file.GetClasses()) {
      for (const art::ClassAccessor::Method& method : accessor.GetMethods()) {
        art::CodeItemInstructionAccessor code = method.GetInstructions();
        if (!code.HasCodeItem()) {
          continue;
        }
        symbols->resize(symbols->size() + 1);
        DexFileSymbol& symbol = symbols->back();
        symbol.offset = reinterpret_cast<const uint8_t*>(code.Insns()) - dex_file.Begin() +
            dex_file_offset;
        symbol.len = code.InsnsSizeInBytes();
        symbol.name = dex_file.PrettyMethod(method.GetIndex(), false);
      }
    }
  };
  return OpenDexFiles(addr, size, dex_file_offsets, dexfile_callback);
}

bool ReadSymbolsFromDexFile(const std::string& file_path,
                            const std::vector<uint64_t>& dex_file_offsets,
                            std::vector<DexFileSymbol>* symbols) {
  android::base::unique_fd fd(TEMP_FAILURE_RETRY(open(file_path.c_str(), O_RDONLY | O_CLOEXEC)));
  if (fd == -1) {
    return false;
  }
  struct stat buf;
  if (fstat(fd, &buf) == -1 || buf.st_size < 0) {
    return false;
  }
  uint64_t file_size = buf.st_size;
  void* addr = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (addr == MAP_FAILED) {
    return false;
  }
  bool result = ReadSymbolsFromDexFileInMemory(addr, file_size, dex_file_offsets, symbols);
  munmap(addr, file_size);
  return result;
}
