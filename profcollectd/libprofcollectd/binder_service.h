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

#include <binder/BinderService.h>
#include <binder/Status.h>

#include "com/android/server/profcollect/BnProfCollectd.h"
#include "scheduler.h"

namespace android {
namespace profcollectd {

class ProfcollectdBinder : public BinderService<ProfcollectdBinder>,
                           public ::com::android::server::profcollect::BnProfCollectd {
 public:
  explicit ProfcollectdBinder();

  static constexpr const char* getServiceName() { return "profcollectd"; }

  binder::Status ReadConfig() override;
  binder::Status ScheduleCollection() override;
  binder::Status TerminateCollection() override;
  binder::Status TraceOnce(const std::string& tag) override;
  binder::Status ProcessProfile() override;
  binder::Status CreateProfileReport() override;
  binder::Status GetSupportedProvider(std::string* provider) override;

 protected:
  inline static std::unique_ptr<ProfcollectdScheduler> Scheduler;
};

}  // namespace profcollectd
}  // namespace android
