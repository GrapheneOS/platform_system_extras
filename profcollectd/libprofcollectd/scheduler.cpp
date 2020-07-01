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

#define LOG_TAG "profcollectd_scheduler"

#include "scheduler.h"

#include <fstream>
#include <variant>
#include <vector>

#include <android-base/logging.h>
#include <android-base/properties.h>

#include "compress.h"
#include "hwtrace_provider.h"
#include "json/json.h"
#include "json/writer.h"

// Default option values.
using config_t = std::pair<const char*, std::variant<const int, const char*>>;
static constexpr const config_t CONFIG_BUILD_FINGERPRINT = {"Fingerprint", "unknown"};
static constexpr const config_t CONFIG_COLLECTION_INTERVAL_SEC = {"CollectionInterval", 600};
static constexpr const config_t CONFIG_SAMPLING_PERIOD_MS = {"SamplingPeriod", 500};
static constexpr const config_t CONFIG_TRACE_OUTDIR = {"TraceDir", "/data/misc/profcollectd/trace"};
static constexpr const config_t CONFIG_PROFILE_OUTDIR = {"ProfileDir",
                                                         "/data/misc/profcollectd/output"};
static constexpr const config_t CONFIG_BINARY_FILTER = {"BinaryFilter", ""};

namespace android {
namespace profcollectd {

namespace fs = std::filesystem;
using ::android::base::GetIntProperty;
using ::android::base::GetProperty;

// Hwtrace provider registry
extern std::unique_ptr<HwtraceProvider> REGISTER_SIMPLEPERF_ETM_PROVIDER();

namespace {

void ClearDir(const fs::path& path) {
  if (fs::exists(path)) {
    for (const auto& entry : fs::directory_iterator(path)) {
      fs::remove_all(entry);
    }
  }
}

bool ClearOnConfigChange(const ProfcollectdScheduler::Config& config) {
  const fs::path configFile = config.profileOutputDir / "config.json";
  ProfcollectdScheduler::Config oldConfig{};

  // Read old config, if exists.
  if (fs::exists(configFile)) {
    std::ifstream ifs(configFile);
    ifs >> oldConfig;
  }

  if (oldConfig != config) {
    LOG(INFO) << "Clearing profiles due to config change.";
    ClearDir(config.traceOutputDir);
    ClearDir(config.profileOutputDir);

    // Write new config.
    std::ofstream ofs(configFile);
    ofs << config;
    return true;
  }
  return false;
}

void PeriodicCollectionWorker(std::future<void> terminationSignal, ProfcollectdScheduler& scheduler,
                              std::chrono::seconds& interval) {
  do {
    scheduler.TraceOnce("periodic");
  } while ((terminationSignal.wait_for(interval)) == std::future_status::timeout);
}

}  // namespace

ProfcollectdScheduler::ProfcollectdScheduler() {
  ReadConfig();

  // Load a registered hardware trace provider.
  if ((hwtracer = REGISTER_SIMPLEPERF_ETM_PROVIDER())) {
    LOG(INFO) << "ETM provider registered.";
    return;
  } else {
    LOG(ERROR) << "No hardware trace provider found for this architecture.";
    exit(EXIT_FAILURE);
  }
}

OptError ProfcollectdScheduler::ReadConfig() {
  if (workerThread != nullptr) {
    static std::string errmsg = "Terminate the collection before refreshing config.";
    return errmsg;
  }

  const std::lock_guard<std::mutex> lock(mu);

  config.buildFingerprint = GetProperty("ro.build.fingerprint", "unknown");
  config.collectionInterval = std::chrono::seconds(
      GetIntProperty("profcollectd.collection_interval",
                     std::get<const int>(CONFIG_COLLECTION_INTERVAL_SEC.second)));
  config.samplingPeriod = std::chrono::milliseconds(GetIntProperty(
      "profcollectd.sampling_period_ms", std::get<const int>(CONFIG_SAMPLING_PERIOD_MS.second)));
  config.traceOutputDir = GetProperty("profcollectd.trace_output_dir",
                                      std::get<const char*>(CONFIG_TRACE_OUTDIR.second));
  config.profileOutputDir =
      GetProperty("profcollectd.output_dir", std::get<const char*>(CONFIG_PROFILE_OUTDIR.second));
  config.binaryFilter =
      GetProperty("profcollectd.binary_filter", std::get<const char*>(CONFIG_BINARY_FILTER.second));
  ClearOnConfigChange(config);

  return std::nullopt;
}

OptError ProfcollectdScheduler::ScheduleCollection() {
  if (workerThread != nullptr) {
    static std::string errmsg = "Collection is already scheduled.";
    return errmsg;
  }

  workerThread =
      std::make_unique<std::thread>(PeriodicCollectionWorker, terminate.get_future(),
                                    std::ref(*this), std::ref(config.collectionInterval));
  return std::nullopt;
}

OptError ProfcollectdScheduler::TerminateCollection() {
  if (workerThread == nullptr) {
    static std::string errmsg = "Collection is not scheduled.";
    return errmsg;
  }

  terminate.set_value();
  workerThread->join();
  workerThread = nullptr;
  terminate = std::promise<void>();  // Reset promise.
  return std::nullopt;
}

OptError ProfcollectdScheduler::TraceOnce(const std::string& tag) {
  const std::lock_guard<std::mutex> lock(mu);
  bool success = hwtracer->Trace(config.traceOutputDir, tag, config.samplingPeriod);
  if (!success) {
    static std::string errmsg = "Trace failed";
    return errmsg;
  }
  return std::nullopt;
}

OptError ProfcollectdScheduler::ProcessProfile() {
  const std::lock_guard<std::mutex> lock(mu);
  hwtracer->Process(config.traceOutputDir, config.profileOutputDir, config.binaryFilter);
  std::vector<fs::path> profiles;
  profiles.insert(profiles.begin(), fs::directory_iterator(config.profileOutputDir),
                  fs::directory_iterator());
  bool success = CompressFiles("/sdcard/profile.zip", profiles);
  if (!success) {
    static std::string errmsg = "Compress files failed";
    return errmsg;
  }
  return std::nullopt;
}

std::ostream& operator<<(std::ostream& os, const ProfcollectdScheduler::Config& config) {
  Json::Value root;
  const auto writer = std::make_unique<Json::StyledStreamWriter>();
  root[CONFIG_BUILD_FINGERPRINT.first] = config.buildFingerprint;
  root[CONFIG_COLLECTION_INTERVAL_SEC.first] = config.collectionInterval.count();
  root[CONFIG_SAMPLING_PERIOD_MS.first] = config.samplingPeriod.count();
  root[CONFIG_TRACE_OUTDIR.first] = config.traceOutputDir.c_str();
  root[CONFIG_PROFILE_OUTDIR.first] = config.profileOutputDir.c_str();
  root[CONFIG_BINARY_FILTER.first] = config.binaryFilter.c_str();
  writer->write(os, root);
  return os;
}

std::istream& operator>>(std::istream& is, ProfcollectdScheduler::Config& config) {
  Json::Value root;
  const auto reader = std::make_unique<Json::Reader>();
  bool success = reader->parse(is, root);
  if (!success) {
    return is;
  }

  config.buildFingerprint = root[CONFIG_BUILD_FINGERPRINT.first].asString();
  config.collectionInterval =
      std::chrono::seconds(root[CONFIG_COLLECTION_INTERVAL_SEC.first].asInt64());
  config.samplingPeriod =
      std::chrono::duration<float>(root[CONFIG_SAMPLING_PERIOD_MS.first].asFloat());
  config.traceOutputDir = root[CONFIG_TRACE_OUTDIR.first].asString();
  config.profileOutputDir = root[CONFIG_PROFILE_OUTDIR.first].asString();
  config.binaryFilter = root[CONFIG_BINARY_FILTER.first].asString();

  return is;
}

}  // namespace profcollectd
}  // namespace android
