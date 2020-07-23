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

#define LOG_TAG "profcolelctd_simpleperf_etm"

#include "hwtrace_provider.h"

#include <android-base/logging.h>
#include <simpleperf_profcollect.h>

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <thread>

static constexpr const char* ETM_TRACEFILE_EXTENSION = ".etmtrace";
static constexpr const char* OUTPUT_FILE_EXTENSION = ".data";

namespace {

std::string GetTimestamp() {
  auto now = std::time(nullptr);
  char timestr[32];
  std::strftime(timestr, sizeof(timestr), "%Y%m%d-%H%M%S", std::localtime(&now));
  return timestr;
}

}  // namespace

namespace android {
namespace profcollectd {

namespace fs = std::filesystem;

class SimpleperfETMProvider : public HwtraceProvider {
 public:
  static bool IsSupported();
  std::string GetName();
  bool Trace(const fs::path& outputPath, const std::string& tag,
             std::chrono::duration<float> samplingPeriod) override;
  bool Process(const fs::path& inputPath, const fs::path& outputPath,
               const std::string& binaryFilter) override;
};

bool SimpleperfETMProvider::IsSupported() {
  return simpleperf::etm::HasSupport();
}

std::string SimpleperfETMProvider::GetName() {
  static constexpr const char* name = "simpleperf_etm";
  return name;
}

bool SimpleperfETMProvider::Trace(const fs::path& outputPath, const std::string& tag,
                                  std::chrono::duration<float> samplingPeriod) {
  const std::string timestamp = GetTimestamp();
  auto outputFile = outputPath / (timestamp + "_" + tag + ETM_TRACEFILE_EXTENSION);
  return simpleperf::etm::Record(outputFile, samplingPeriod);
}

bool SimpleperfETMProvider::Process(const fs::path& inputPath, const fs::path& outputPath,
                                    const std::string& binaryFilter) {
  for (const auto& entry : fs::directory_iterator(inputPath)) {
    const fs::path& traceFile = entry.path();
    if (traceFile.extension() != ETM_TRACEFILE_EXTENSION) {
      continue;
    }

    const fs::path binaryOutput =
        outputPath / traceFile.filename().replace_extension(OUTPUT_FILE_EXTENSION);

    bool success = simpleperf::etm::Inject(traceFile, binaryOutput, binaryFilter);
    if (success) {
      fs::remove(traceFile);
    }
  }

  return true;
}

std::unique_ptr<HwtraceProvider> REGISTER_SIMPLEPERF_ETM_PROVIDER() {
  if (SimpleperfETMProvider::IsSupported()) {
    return std::make_unique<SimpleperfETMProvider>();
  }
  return nullptr;
}

}  // namespace profcollectd
}  // namespace android
