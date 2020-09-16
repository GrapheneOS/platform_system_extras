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

#include <sysexits.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <gflags/gflags.h>
#include <json/json.h>
#include <kver/kernel_release.h>
#include <kver/kmi_version.h>
#include <kver/utils.h>

using android::kver::GetFactoryApexVersion;
using android::kver::KernelRelease;
using android::kver::KmiVersion;

namespace {

int CheckKmi(const KernelRelease& kernel_release, const std::string& kmi_version) {
  const auto& actual_kmi_version = kernel_release.kmi_version().string();
  if (actual_kmi_version != kmi_version) {
    LOG(ERROR) << "KMI version does not match. Actual: " << actual_kmi_version
               << ", expected: " << kmi_version;
    return EX_SOFTWARE;
  }
  if (kernel_release.sub_level() == GetFactoryApexVersion()) {
    LOG(ERROR) << "Kernel release is " << kernel_release.string() << ". Sub-level "
               << GetFactoryApexVersion() << " is reserved for factory GKI APEX.";
    return EX_SOFTWARE;
  }
  return EX_OK;
}

int WriteApexManifest(const std::string& apex_name, Json::UInt64 apex_version,
                      const std::string& out_file) {
  Json::Value root;
  root["name"] = apex_name;
  root["version"] = apex_version;
  root["preInstallHook"] = "bin/com.android.gki.preinstall";
  root["postInstallHook"] = "bin/com.android.gki.postinstall";
  std::string json_string = Json::StyledWriter().write(root);
  if (!android::base::WriteStringToFile(json_string, out_file)) {
    PLOG(ERROR) << "Cannot write to " << out_file;
    return EX_SOFTWARE;
  }
  return EX_OK;
}

}  // namespace

DEFINE_string(kernel_release_file, "",
              "Input file that contains a kernel release string parsed from the boot image. "
              "Exactly one of --kernel_release_file or --factory must be set.");
DEFINE_bool(factory, false,
            "Set to true for factory APEX package. Exactly one of --kernel_release_file or "
            "--factory must be set.");
DEFINE_string(kmi_version, "", "Declared KMI version for this APEX.");
DEFINE_string(apex_manifest, "", "Output APEX manifest JSON file.");

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_kmi_version.empty()) {
    LOG(ERROR) << "--kmi_version must be set.";
    return EX_SOFTWARE;
  }
  auto kmi_version = KmiVersion::Parse(FLAGS_kmi_version);
  if (!kmi_version.has_value()) {
    LOG(ERROR) << "--kmi_version is not a valid KMI version.";
    return EX_SOFTWARE;
  }

  if (FLAGS_factory + (!FLAGS_kernel_release_file.empty()) != 1) {
    LOG(ERROR) << "Exactly one of --kernel_release_file or --factory must be set.";
    return EX_SOFTWARE;
  }

  std::string apex_name;
  uint64_t apex_version;
  if (FLAGS_factory) {
    apex_name = GetApexName(*kmi_version);
    apex_version = GetFactoryApexVersion();
  } else {
    std::string kernel_release_string;
    if (!android::base::ReadFileToString(FLAGS_kernel_release_file, &kernel_release_string)) {
      PLOG(ERROR) << "Cannot read " << FLAGS_kernel_release_file;
      return EX_SOFTWARE;
    }
    auto kernel_release = KernelRelease::Parse(kernel_release_string, true /* allow_suffix */);
    if (!kernel_release.has_value()) {
      LOG(ERROR) << kernel_release_string << " is not a valid GKI kernel release string";
      return EX_SOFTWARE;
    }
    int res = CheckKmi(*kernel_release, FLAGS_kmi_version);
    if (res != EX_OK) return res;

    apex_name = GetApexName(kernel_release->kmi_version());
    apex_version = GetApexVersion(*kernel_release);
  }

  if (FLAGS_apex_manifest.empty()) {
    LOG(WARNING) << "Skip writing APEX manifest because --apex_manifest is not set.";
  } else {
    int res = WriteApexManifest(apex_name, apex_version, FLAGS_apex_manifest);
    if (res != EX_OK) return res;
  }

  return EX_OK;
}
