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

#include "config_utils.h"

#include <android-base/properties.h>
#include <server_configurable_flags/get_flags.h>

static const std::string PROFCOLLECT_CONFIG_NAMESPACE = "profcollect_native_boot";

namespace android {
namespace profcollectd {

using ::android::base::GetProperty;
using ::server_configurable_flags::GetServerConfigurableFlag;

std::string getBuildFingerprint() {
  return GetProperty("ro.build.fingerprint", "unknown");
}

std::string getConfigFlag(const config_t& config) {
  return GetServerConfigurableFlag(PROFCOLLECT_CONFIG_NAMESPACE, config.name, config.defaultValue);
}

int getConfigFlagInt(const config_t& config) {
  std::string value = getConfigFlag(config);
  return std::stoi(value);
}

float getConfigFlagFloat(const config_t& config) {
  std::string value = getConfigFlag(config);
  return std::stof(value);
}

}  // namespace profcollectd
}  // namespace android
