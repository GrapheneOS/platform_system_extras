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

#define LOG_TAG "profcollectd_binder"

#include "binder_service.h"

#include <android-base/logging.h>

#include "config_utils.h"
#include "hwtrace_provider.h"
#include "libprofcollectd.hpp"
#include "scheduler.h"

namespace android {
namespace profcollectd {

using ::android::binder::Status;
using ::android::profcollectd::ProfcollectdBinder;
using ::com::android::server::profcollect::IProfCollectd;

namespace {

static constexpr const char* NOT_ENABLED_ERRMSG =
    "profcollectd is not enabled through device config.";
static constexpr config_t CONFIG_ENABLED = {"enabled", "0"};  // Disabled by default.

}  // namespace

void InitService(bool start) {
  if (defaultServiceManager()->checkService(String16(ProfcollectdBinder::getServiceName()))) {
    ALOGE("Another instance of profcollectd is already running");
    exit(EXIT_FAILURE);
  }

  sp<ProcessState> proc(ProcessState::self());
  sp<IServiceManager> sm(defaultServiceManager());
  auto svc = sp<ProfcollectdBinder>(new ProfcollectdBinder());
  sm->addService(String16(ProfcollectdBinder::getServiceName()), svc);
  if (start) {
    svc->ScheduleCollection();
  }
  ProcessState::self()->startThreadPool();
  IPCThreadState::self()->joinThreadPool();
}

Status ProfcollectdBinder::ForwardScheduler(const std::function<OptError()>& action) {
  if (Scheduler == nullptr) {
    return Status::fromExceptionCode(1, NOT_ENABLED_ERRMSG);
  }

  auto errmsg = action();
  if (errmsg) {
    LOG(ERROR) << errmsg.value();
    return Status::fromExceptionCode(1, errmsg.value().c_str());
  }
  return Status::ok();
}

ProfcollectdBinder::ProfcollectdBinder() {
  static bool enabled = getConfigFlagBool(CONFIG_ENABLED);

  if (enabled) {
    ProfcollectdBinder::Scheduler = std::make_unique<ProfcollectdScheduler>();
    LOG(INFO) << "Binder service started";
  } else {
    LOG(INFO) << NOT_ENABLED_ERRMSG;
  }
}

Status ProfcollectdBinder::ReadConfig() {
  return ForwardScheduler([=]() { return Scheduler->ReadConfig(); });
}

Status ProfcollectdBinder::ScheduleCollection() {
  return ForwardScheduler([=]() { return Scheduler->ScheduleCollection(); });
}

Status ProfcollectdBinder::TerminateCollection() {
  return ForwardScheduler([=]() { return Scheduler->TerminateCollection(); });
}

Status ProfcollectdBinder::TraceOnce(const std::string& tag) {
  return ForwardScheduler([=]() { return Scheduler->TraceOnce(tag); });
}

Status ProfcollectdBinder::ProcessProfile() {
  return ForwardScheduler([=]() { return Scheduler->ProcessProfile(); });
}

Status ProfcollectdBinder::CreateProfileReport() {
  return ForwardScheduler([=]() { return Scheduler->CreateProfileReport(); });
}

Status ProfcollectdBinder::GetSupportedProvider(std::string* provider) {
  return ForwardScheduler([=]() { return Scheduler->GetSupportedProvider(*provider); });
}

}  // namespace profcollectd
}  // namespace android
