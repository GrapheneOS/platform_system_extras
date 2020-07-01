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

#include "hwtrace_provider.h"
#include "scheduler.h"

namespace android {
namespace profcollectd {

using ::android::binder::Status;
using ::com::android::server::profcollect::IProfCollectd;

namespace {

Status HandleIfError(const std::function<OptError()>& action) {
  auto errmsg = action();
  if (errmsg) {
    LOG(ERROR) << errmsg.value();
    return Status::fromExceptionCode(1, errmsg.value().c_str());
  }
  return Status::ok();
}

}  // namespace

ProfcollectdBinder::ProfcollectdBinder() {
  ProfcollectdBinder::Scheduler = std::make_unique<ProfcollectdScheduler>();
  LOG(INFO) << "Binder service started";
}

Status ProfcollectdBinder::ReadConfig() {
  return HandleIfError([=]() { return Scheduler->ReadConfig(); });
}

Status ProfcollectdBinder::ScheduleCollection() {
  return HandleIfError([=]() { return Scheduler->ScheduleCollection(); });
}

Status ProfcollectdBinder::TerminateCollection() {
  return HandleIfError([=]() { return Scheduler->TerminateCollection(); });
}

Status ProfcollectdBinder::TraceOnce() {
  return HandleIfError([=]() { return Scheduler->TraceOnce(); });
}

Status ProfcollectdBinder::ProcessProfile() {
  return HandleIfError([=]() { return Scheduler->ProcessProfile(); });
}

}  // namespace profcollectd
}  // namespace android
