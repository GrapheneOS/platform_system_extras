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

#include <android-base/parsedouble.h>
#include <android-base/parseint.h>
#include <android-base/properties.h>
#include <server_configurable_flags/get_flags.h>

static const std::string PROFCOLLECT_CONFIG_NAMESPACE = "profcollect_native_boot";

namespace android {
namespace profcollectd {

using ::android::base::GetProperty;
using ::android::base::ParseFloat;
using ::android::base::ParseInt;
using ::server_configurable_flags::GetServerConfigurableFlag;

std::string getBuildFingerprint() {
  return GetProperty("ro.build.fingerprint", "unknown");
}

std::string getConfigFlag(const config_t& config) {
  return GetServerConfigurableFlag(PROFCOLLECT_CONFIG_NAMESPACE, config.name, config.defaultValue);
}

int getConfigFlagInt(const config_t& config) {
  std::string value = getConfigFlag(config);
  int i;
  if (!ParseInt(value, &i)) {
    // Use default value if the server config value is malformed.
    return ParseInt(config.defaultValue, &i);
  }
  return i;
}

float getConfigFlagFloat(const config_t& config) {
  std::string value = getConfigFlag(config);
  float f;
  if (!ParseFloat(value, &f)) {
    // Use default value if the server config value is malformed.
    return ParseFloat(config.defaultValue, &f);
  }
  return f;
}

bool getConfigFlagBool(const config_t& config) {
  std::string value = getConfigFlag(config);
  return value == "true";
}

}  // namespace profcollectd
}  // namespace android
