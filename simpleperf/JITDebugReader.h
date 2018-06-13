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
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <android-base/logging.h>
#include <android-base/test_utils.h>

#include "IOEventLoop.h"
#include "record.h"

namespace simpleperf {

struct JITSymFile {
  pid_t pid;  // The process having the JITed code
  uint64_t addr;  // The start addr of the JITed code
  uint64_t len;  // The length of the JITed code
  std::string file_path;  // The path of a temporary ELF file storing debug info of the JITed code

  JITSymFile() {}
  JITSymFile(pid_t pid, uint64_t addr, uint64_t len, const std::string& file_path)
      : pid(pid), addr(addr), len(len), file_path(file_path) {}
};

struct DexSymFile {
  uint64_t dex_file_offset;  // The offset of the dex file in the file containing it
  std::string file_path;  // The path of file containing the dex file

  DexSymFile() {}
  DexSymFile(uint64_t dex_file_offset, const std::string& file_path)
      : dex_file_offset(dex_file_offset), file_path(file_path) {}
};

// JITDebugReader reads debug info of JIT code and dex files of processes using ART. The
// corresponding debug interface in ART is at art/runtime/jit/debugger_interface.cc.
class JITDebugReader {
 public:
  JITDebugReader(bool keep_symfiles) : keep_symfiles_(keep_symfiles) {}

  typedef std::function<bool(const std::vector<JITSymFile>&, const std::vector<DexSymFile>&, bool)>
      symfile_callback_t;
  bool RegisterSymFileCallback(IOEventLoop* loop, const symfile_callback_t& callback);

  // There are two ways to select which processes to monitor. One is using MonitorProcess(), the
  // other is finding all processes having libart.so using records.
  bool MonitorProcess(pid_t pid);
  bool UpdateRecord(const Record* record);

  // Read new debug info from all monitored processes.
  bool ReadAllProcesses();

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

  struct Process {
    pid_t pid = -1;
    bool initialized = false;
    bool died = false;
    bool is_64bit = false;
    // The jit descriptor and dex descriptor can be read in one process_vm_readv() call.
    uint64_t descriptors_addr = 0;
    uint64_t descriptors_size = 0;
    // offset relative to descriptors_addr
    uint64_t jit_descriptor_offset = 0;
    // offset relative to descriptors_addr
    uint64_t dex_descriptor_offset = 0;

    // The state we know about the remote jit debug descriptor.
    Descriptor last_jit_descriptor;
    // The state we know about the remote dex debug descriptor.
    Descriptor last_dex_descriptor;
  };

  // The location of descriptors in libart.so.
  struct DescriptorsLocation {
    uint64_t relative_addr = 0;
    uint64_t size = 0;
    uint64_t jit_descriptor_offset = 0;
    uint64_t dex_descriptor_offset = 0;
  };

  bool ReadProcess(pid_t pid);
  void ReadProcess(Process& process, std::vector<JITSymFile>* jit_symfiles,
                   std::vector<DexSymFile>* dex_symfiles);
  bool InitializeProcess(Process& process);
  const DescriptorsLocation* GetDescriptorsLocation(const std::string& art_lib_path,
                                                    bool is_64bit);
  bool ReadRemoteMem(Process& process, uint64_t remote_addr, uint64_t size, void* data);
  bool ReadDescriptors(Process& process, Descriptor* jit_descriptor, Descriptor* dex_descriptor);
  bool LoadDescriptor(bool is_64bit, const char* data, Descriptor* descriptor);
  template <typename DescriptorT, typename CodeEntryT>
  bool LoadDescriptorImpl(const char* data, Descriptor* descriptor);

  bool ReadNewCodeEntries(Process& process, const Descriptor& descriptor,
                          uint64_t last_action_timestamp, uint32_t read_entry_limit,
                          std::vector<CodeEntry>* new_code_entries);
  template <typename DescriptorT, typename CodeEntryT>
  bool ReadNewCodeEntriesImpl(Process& process, const Descriptor& descriptor,
                              uint64_t last_action_timestamp, uint32_t read_entry_limit,
                              std::vector<CodeEntry>* new_code_entries);

  void ReadJITSymFiles(Process& process, const std::vector<CodeEntry>& jit_entries,
                       std::vector<JITSymFile>* jit_symfiles);
  void ReadDexSymFiles(Process& process, const std::vector<CodeEntry>& dex_entries,
                       std::vector<DexSymFile>* dex_symfiles);

  bool keep_symfiles_ = false;
  IOEventRef read_event_ = nullptr;
  symfile_callback_t symfile_callback_;

  // Keys are pids of processes having libart.so, values show whether a process has been monitored.
  std::unordered_map<pid_t, bool> pids_with_art_lib_;

  // All monitored processes
  std::unordered_map<pid_t, Process> processes_;
  std::unordered_map<std::string, DescriptorsLocation> descriptors_location_cache_;
  std::vector<char> descriptors_buf_;
};

}  //namespace simpleperf

#endif   // SIMPLE_PERF_JIT_DEBUG_READER_H_
