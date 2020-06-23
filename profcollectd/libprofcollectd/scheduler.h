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

#include <compare>
#include <future>
#include <mutex>
#include <optional>
#include <thread>

#include "hwtrace_provider.h"

using OptError = std::optional<std::string>;

namespace android {
namespace profcollectd {

class ProfcollectdScheduler {
 public:
  struct Config {
    std::string buildFingerprint;
    std::chrono::seconds collectionInterval;
    std::chrono::duration<float> samplingPeriod;
    std::filesystem::path traceOutputDir;
    std::filesystem::path profileOutputDir;
    std::string binaryFilter;

    auto operator<=>(const Config&) const = default;
    friend std::ostream& operator<<(std::ostream& os, const Config& config);
    friend std::istream& operator>>(std::istream& is, Config& config);
  };

  explicit ProfcollectdScheduler();

  // Methods below returns optional error message on failure, otherwise returns std::nullopt.
  OptError ReadConfig();
  OptError ScheduleCollection();
  OptError TerminateCollection();
  OptError TraceOnce();
  OptError ProcessProfile();

 private:
  Config config;
  std::promise<void> terminate;
  std::unique_ptr<HwtraceProvider> hwtracer;
  std::unique_ptr<std::thread> workerThread;
  std::mutex mu;
};

}  // namespace profcollectd
}  // namespace android