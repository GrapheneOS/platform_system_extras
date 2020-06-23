//
// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once

#include <chrono>
#include <filesystem>
#include <functional>

namespace android {
namespace profcollectd {

class HwtraceProvider {
 public:
  virtual ~HwtraceProvider() = default;

  /**
   * Trace for the given length of time.
   *
   * @param period Length of time to trace in seconds.
   * @return True if successful.
   */
  virtual bool Trace(const std::filesystem::path& outputPath,
                     std::chrono::duration<float> samplingPeriod) = 0;

  /**
   * Process the hardware trace to generate simpleperf intermediate profile.
   */
  virtual bool Process(const std::filesystem::path& inputPath,
                       const std::filesystem::path& outputPath,
                       const std::string& binaryFilter) = 0;
};

}  // namespace profcollectd
}  // namespace android