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

namespace fs = std::filesystem;

// Default option values.
using config_t = std::pair<const char*, std::variant<const int, const char*>>;
static constexpr const config_t CONFIG_BUILD_FINGERPRINT = {"Fingerprint", "unknown"};
static constexpr const config_t CONFIG_COLLECTION_INTERVAL_SEC = {"CollectionInterval", 600};
static constexpr const config_t CONFIG_SAMPLING_PERIOD_MS = {"SamplingPeriod", 500};
static constexpr const config_t CONFIG_BINARY_FILTER = {"BinaryFilter", ""};

static const fs::path OUT_ROOT_DIR("/data/misc/profcollectd");
static const fs::path TRACE_DIR(OUT_ROOT_DIR / "trace");
static const fs::path OUTPUT_DIR(OUT_ROOT_DIR / "output");
static const fs::path REPORT_FILE(OUT_ROOT_DIR / "report.zip");

namespace android {
namespace profcollectd {

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
  const fs::path configFile = OUTPUT_DIR / "config.json";
  ProfcollectdScheduler::Config oldConfig{};

  // Read old config, if exists.
  if (fs::exists(configFile)) {
    std::ifstream ifs(configFile);
    ifs >> oldConfig;
  }

  if (oldConfig != config) {
    LOG(INFO) << "Clearing profiles due to config change.";
    ClearDir(TRACE_DIR);
    ClearDir(OUTPUT_DIR);

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
  } else {
    LOG(ERROR) << "No hardware trace provider found for this architecture.";
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
      GetIntProperty("persist.profcollectd.collection_interval",
                     std::get<const int>(CONFIG_COLLECTION_INTERVAL_SEC.second)));
  config.samplingPeriod = std::chrono::milliseconds(GetIntProperty(
      "persist.profcollectd.sampling_period_ms",
      std::get<const int>(CONFIG_SAMPLING_PERIOD_MS.second)));
  config.binaryFilter =
      GetProperty("persist.profcollectd.binary_filter",
                  std::get<const char*>(CONFIG_BINARY_FILTER.second));
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
  if(!hwtracer) {
    return "No trace provider registered.";
  }

  const std::lock_guard<std::mutex> lock(mu);
  bool success = hwtracer->Trace(TRACE_DIR, tag, config.samplingPeriod);
  if (!success) {
    static std::string errmsg = "Trace failed";
    return errmsg;
  }
  return std::nullopt;
}

OptError ProfcollectdScheduler::ProcessProfile() {
  if(!hwtracer) {
    return "No trace provider registered.";
  }

  const std::lock_guard<std::mutex> lock(mu);
  bool success =
      hwtracer->Process(TRACE_DIR, OUTPUT_DIR, config.binaryFilter);
  if (!success) {
    static std::string errmsg = "Process profiles failed";
    return errmsg;
  }
  return std::nullopt;
}

OptError ProfcollectdScheduler::CreateProfileReport() {
  auto processFailureMsg = ProcessProfile();
  if (processFailureMsg) {
    return processFailureMsg;
  }

  std::vector<fs::path> profiles;
  if (fs::exists(OUTPUT_DIR)) {
    profiles.insert(profiles.begin(), fs::directory_iterator(OUTPUT_DIR),
                    fs::directory_iterator());
  }
  bool success = CompressFiles(REPORT_FILE, profiles);
  if (!success) {
    static std::string errmsg = "Compress files failed";
    return errmsg;
  }
  return std::nullopt;
}

OptError ProfcollectdScheduler::GetSupportedProvider(std::string& provider) {
  provider = hwtracer ? hwtracer->GetName() : "";
  return std::nullopt;
}

std::ostream& operator<<(std::ostream& os, const ProfcollectdScheduler::Config& config) {
  Json::Value root;
  const auto writer = std::make_unique<Json::StyledStreamWriter>();
  root[CONFIG_BUILD_FINGERPRINT.first] = config.buildFingerprint;
  root[CONFIG_COLLECTION_INTERVAL_SEC.first] = config.collectionInterval.count();
  root[CONFIG_SAMPLING_PERIOD_MS.first] = config.samplingPeriod.count();
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
  config.binaryFilter = root[CONFIG_BINARY_FILTER.first].asString();

  return is;
}

}  // namespace profcollectd
}  // namespace android
