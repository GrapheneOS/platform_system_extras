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

#ifndef SIMPLE_PERF_JIT_DEBUG_READER_H_
#define SIMPLE_PERF_JIT_DEBUG_READER_H_

#include <unistd.h>

#include <functional>
#include <memory>
#include <stack>
#include <unordered_set>
#include <vector>

#include <android-base/logging.h>
#include <android-base/test_utils.h>

namespace simpleperf {

struct JITSymFile {
  uint64_t addr;  // The start addr of the JITed code
  uint64_t len;  // The length of the JITed code
  std::string file_path;  // The path of a temporary ELF file storing debug info of the JITed code
};

struct DexSymFile {
  uint64_t dex_file_offset;  // The offset of the dex file in the file containing it
  std::string file_path;  // The path of file containing the dex file
};

class JITDebugReader {
 public:
  JITDebugReader(pid_t pid, bool keep_symfiles);

  pid_t Pid() const {
    return pid_;
  }

  void ReadUpdate(std::vector<JITSymFile>* new_jit_symfiles,
                  std::vector<DexSymFile>* new_dex_symfiles);

 private:

  // An arch-independent representation of JIT/dex debug descriptor.
  struct Descriptor {
    uint32_t action_seqlock = 0;  // incremented before and after any modification
    uint64_t action_timestamp = 0;  // CLOCK_MONOTONIC time of last action
    uint64_t first_entry_addr = 0;
  };

  // An arch-independent representation of JIT/dex code entry.
  struct CodeEntry {
    uint64_t addr;
    uint64_t symfile_addr;
    uint64_t symfile_size;
    uint64_t timestamp;  // CLOCK_MONOTONIC time of last action
  };

  bool TryInit();
  bool ReadRemoteMem(uint64_t remote_addr, uint64_t size, void* data);
  bool ReadDescriptors(Descriptor* jit_descriptor, Descriptor* dex_descriptor);
  bool LoadDescriptor(const char* data, Descriptor* descriptor);
  template <typename DescriptorT>
  bool LoadDescriptorImpl(const char* data, Descriptor* descriptor);

  bool ReadNewCodeEntries(const Descriptor& descriptor, uint64_t last_action_timestamp,
                          std::vector<CodeEntry>* new_code_entries);
  template <typename DescriptorT, typename CodeEntryT>
  bool ReadNewCodeEntriesImpl(const Descriptor& descriptor, uint64_t last_action_timestamp,
                              std::vector<CodeEntry>* new_code_entries);

  void ReadJITSymFiles(const std::vector<CodeEntry>& jit_entries,
                       std::vector<JITSymFile>* jit_symfiles);
  void ReadDexSymFiles(const std::vector<CodeEntry>& dex_entries,
                       std::vector<DexSymFile>* dex_symfiles);

  pid_t pid_;
  bool keep_symfiles_;
  bool initialized_;
  bool is_64bit_;

  // The jit descriptor and dex descriptor can be read in one process_vm_readv() call.
  uint64_t descriptors_addr_;
  uint64_t descriptors_size_;
  std::vector<char> descriptors_buf_;
  // offset relative to descriptors_addr
  uint64_t jit_descriptor_offset_;
  // offset relative to descriptors_addr
  uint64_t dex_descriptor_offset_;

  // The state we know about the remote jit debug descriptor.
  Descriptor last_jit_descriptor_;
  // The state we know about the remote dex debug descriptor.
  Descriptor last_dex_descriptor_;
};

}  //namespace simpleperf

#endif   // SIMPLE_PERF_JIT_DEBUG_READER_H_
