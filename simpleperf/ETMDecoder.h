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

#pragma once

#include <memory>
#include <string>

#include "record.h"
#include "thread_tree.h"

namespace simpleperf {

struct ETMDumpOption {
  bool dump_raw_data = false;
  bool dump_packets = false;
  bool dump_elements = false;
};

bool ParseEtmDumpOption(const std::string& s, ETMDumpOption* option);

class ETMDecoder {
 public:
  static std::unique_ptr<ETMDecoder> Create(const AuxTraceInfoRecord& auxtrace_info,
                                            ThreadTree& thread_tree);
  virtual ~ETMDecoder() {}
  virtual void EnableDump(const ETMDumpOption& option) = 0;
  virtual bool ProcessData(const uint8_t* data, size_t size) = 0;
};

}  // namespace