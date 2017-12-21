/*
**
** Copyright 2017, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef SYSTEM_EXTRAS_PERFPROFD_PERFPROFD_BINDER_H_
#define SYSTEM_EXTRAS_PERFPROFD_PERFPROFD_BINDER_H_

#include <cstdlib>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <inttypes.h>
#include <unistd.h>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <binder/BinderService.h>
#include <binder/IResultReceiver.h>
#include <binder/Status.h>
#include <utils/String16.h>
#include <utils/String8.h>
#include <utils/Vector.h>

#include "android/os/BnPerfProfd.h"

#include "config.h"
#include "configreader.h"
#include "perfprofdcore.h"

namespace android {
namespace perfprofd {

class PerfProfdNativeService : public BinderService<PerfProfdNativeService>,
                               public os::BnPerfProfd {
 public:
  static status_t start() {
    IPCThreadState::self()->disableBackgroundScheduling(true);
    status_t ret = BinderService<PerfProfdNativeService>::publish();
    if (ret != android::OK) {
      return ret;
    }
    sp<ProcessState> ps(ProcessState::self());
    ps->startThreadPool();
    ps->giveThreadPoolName();
    return android::OK;
  }

  static char const* getServiceName() { return "perfprofd"; }

  status_t dump(int fd, const Vector<String16> &args) override {
    auto out = std::fstream(base::StringPrintf("/proc/self/fd/%d", fd));
    out << "Nothing to log, yet!" << std::endl;

    return NO_ERROR;
  }

  binder::Status startProfiling(int32_t profilingDuration,
                                int32_t profilingInterval,
                                int32_t iterations) override {
    UNIMPLEMENTED(ERROR) << "startProfiling: To be implemented";
    return binder::Status::fromExceptionCode(1);
  }

  binder::Status stopProfiling() override {
    UNIMPLEMENTED(ERROR) << "stopProfiling: To be implemented";
    return binder::Status::fromExceptionCode(1);
  }

  // Override onTransact so we can handle shellCommand.
  status_t onTransact(uint32_t _aidl_code,
                      const Parcel& _aidl_data,
                      Parcel* _aidl_reply,
                      uint32_t _aidl_flags = 0) override {
    switch (_aidl_code) {
      case IBinder::SHELL_COMMAND_TRANSACTION: {
        int in = _aidl_data.readFileDescriptor();
        int out = _aidl_data.readFileDescriptor();
        int err = _aidl_data.readFileDescriptor();
        int argc = _aidl_data.readInt32();
        Vector<String16> args;
        for (int i = 0; i < argc && _aidl_data.dataAvail() > 0; i++) {
          args.add(_aidl_data.readString16());
        }
        sp<IBinder> unusedCallback;
        sp<IResultReceiver> resultReceiver;
        status_t status;
        if ((status = _aidl_data.readNullableStrongBinder(&unusedCallback)) != OK)
          return status;
        if ((status = _aidl_data.readNullableStrongBinder(&resultReceiver)) != OK)
          return status;
        status = shellCommand(in, out, err, args);
        if (resultReceiver != nullptr) {
          resultReceiver->send(status);
        }
        return OK;
      }

      default:
        return BBinder::onTransact(_aidl_code, _aidl_data, _aidl_reply, _aidl_flags);
    }
  }

 private:
  status_t shellCommand(int /*in*/, int out, int err, Vector<String16>& args) {
    if (android::base::kEnableDChecks) {
      LOG(VERBOSE) << "Perfprofd::shellCommand";

      for (size_t i = 0, n = args.size(); i < n; i++) {
        LOG(VERBOSE) << "  arg[" << i << "]: '" << String8(args[i]).string() << "'";
      }
    }

    if (args.size() >= 1) {
      if (args[0] == String16("dump")) {
        dump(out, args);
        return OK;
      } else if (args[0] == String16("startProfiling")) {
        if (args.size() < 4) {
          return BAD_VALUE;
        }
        // TODO: handle invalid strings.
        int32_t duration = strtol(String8(args[1]).string(), nullptr, 0);
        int32_t interval = strtol(String8(args[2]).string(), nullptr, 0);
        int32_t iterations = strtol(String8(args[3]).string(), nullptr, 0);
        binder::Status status = startProfiling(duration, interval, iterations);
        if (status.isOk()) {
          return OK;
        } else {
          return status.serviceSpecificErrorCode();
        }
      } else if (args[0] == String16("stopProfiling")) {
        binder::Status status = stopProfiling();
        if (status.isOk()) {
          return OK;
        } else {
          return status.serviceSpecificErrorCode();
        }
      }
    }
    return BAD_VALUE;
  }
};

}  // namespace perfprofd
}  // namespace android

#endif  // SYSTEM_EXTRAS_PERFPROFD_PERFPROFD_BINDER_H_
