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

#define LOG_TAG "profcollectd"

#include "libprofcollectd.hpp"

#include <android-base/logging.h>

#include <iostream>

#include "binder_service.h"

using com::android::server::profcollect::IProfCollectd;
using android::profcollectd::ProfcollectdBinder;

using namespace android;

namespace {

sp<IProfCollectd> GetIProfcollectdService() {
  sp<ProcessState> proc(ProcessState::self());
  sp<IBinder> binder =
      defaultServiceManager()->getService(String16(ProfcollectdBinder::getServiceName()));
  if (!binder) {
    std::cerr << "Cannot connect to the daemon, is it running?" << std::endl;
    exit(EXIT_FAILURE);
  }

  return interface_cast<IProfCollectd>(binder);
}

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

void ScheduleCollection() {
  GetIProfcollectdService()->ScheduleCollection();
}

void TerminateCollection() {
  GetIProfcollectdService()->TerminateCollection();
}

void TraceOnce() {
  GetIProfcollectdService()->TraceOnce("manual");
}

void Process() {
  GetIProfcollectdService()->ProcessProfile();
}

void CreateProfileReport() {
  GetIProfcollectdService()->CreateProfileReport();
}

void ReadConfig() {
  GetIProfcollectdService()->ReadConfig();
}
