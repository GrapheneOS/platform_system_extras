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

#include "perfprofd_binder.h"

#include <chrono>
#include <condition_variable>
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
namespace binder {

using Status = ::android::binder::Status;

class BinderConfig : public Config {
 public:
  bool is_profiling = false;

  void Sleep(size_t seconds) override {
    std::unique_lock<std::mutex> guard(mutex_);
    using namespace std::chrono_literals;
    cv_.wait_for(guard, seconds * 1s, [&]() { return interrupted_; });
  }
  bool ShouldStopProfiling() override {
    std::unique_lock<std::mutex> guard(mutex_);
    return interrupted_;
  }

  void ResetStopProfiling() {
    std::unique_lock<std::mutex> guard(mutex_);
    interrupted_ = false;
  }
  void StopProfiling() {
    std::unique_lock<std::mutex> guard(mutex_);
    interrupted_ = true;
    cv_.notify_all();
  }

 private:
  std::mutex mutex_;
  std::condition_variable cv_;
  bool interrupted_ = false;
};

class PerfProfdNativeService : public BinderService<PerfProfdNativeService>,
                               public ::android::os::BnPerfProfd {
 public:
  static status_t start();
  static int Main();

  static char const* getServiceName() { return "perfprofd"; }

  status_t dump(int fd, const Vector<String16> &args) override;

  Status startProfiling(int32_t profilingDuration,
                                int32_t profilingInterval,
                                int32_t iterations) override;

  Status stopProfiling() override;

  // Override onTransact so we can handle shellCommand.
  status_t onTransact(uint32_t _aidl_code,
                      const Parcel& _aidl_data,
                      Parcel* _aidl_reply,
                      uint32_t _aidl_flags = 0) override;

 private:
  status_t shellCommand(int /*in*/, int out, int err, Vector<String16>& args);

  std::mutex lock_;

  BinderConfig cur_config_;
};

status_t PerfProfdNativeService::start() {
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

status_t PerfProfdNativeService::dump(int fd, const Vector<String16> &args) {
  auto out = std::fstream(base::StringPrintf("/proc/self/fd/%d", fd));
  out << "Nothing to log, yet!" << std::endl;

  return NO_ERROR;
}

Status PerfProfdNativeService::startProfiling(int32_t profilingDuration,
                                                      int32_t profilingInterval,
                                                      int32_t iterations) {
  std::lock_guard<std::mutex> guard(lock_);

  if (cur_config_.is_profiling) {
    // TODO: Define error code?
    return Status::fromServiceSpecificError(1);
  }
  cur_config_.is_profiling = true;
  cur_config_.ResetStopProfiling();

  ConfigReader().FillConfig(&cur_config_);  // Create a default config.

  cur_config_.sample_duration_in_s = static_cast<uint32_t>(profilingDuration);
  cur_config_.collection_interval_in_s = static_cast<uint32_t>(profilingInterval);
  cur_config_.main_loop_iterations = static_cast<uint32_t>(iterations);

  auto profile_runner = [](PerfProfdNativeService* service) {
    ProfilingLoop(service->cur_config_);

    // This thread is done.
    std::lock_guard<std::mutex> unset_guard(service->lock_);
    service->cur_config_.is_profiling = false;
  };
  std::thread profiling_thread(profile_runner, this);
  profiling_thread.detach();  // Let it go.

  return Status::ok();
}

Status PerfProfdNativeService::stopProfiling() {
  std::lock_guard<std::mutex> guard(lock_);
  if (!cur_config_.is_profiling) {
    // TODO: Define error code?
    return Status::fromServiceSpecificError(1);
  }

  cur_config_.StopProfiling();

  return Status::ok();
}

status_t PerfProfdNativeService::shellCommand(int /*in*/,
                                              int out,
                                              int err,
                                              Vector<String16>& args) {
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
      Status status = startProfiling(duration, interval, iterations);
      if (status.isOk()) {
        return OK;
      } else {
        return status.serviceSpecificErrorCode();
      }
    } else if (args[0] == String16("stopProfiling")) {
      Status status = stopProfiling();
      if (status.isOk()) {
        return OK;
      } else {
        return status.serviceSpecificErrorCode();
      }
    }
  }
  return BAD_VALUE;
}

status_t PerfProfdNativeService::onTransact(uint32_t _aidl_code,
                                            const Parcel& _aidl_data,
                                            Parcel* _aidl_reply,
                                            uint32_t _aidl_flags) {
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

int Main() {
  android::status_t ret;
  if ((ret = PerfProfdNativeService::start()) != android::OK) {
    LOG(ERROR) << "Unable to start InstalldNativeService: %d" << ret;
    exit(1);
  }

  android::IPCThreadState::self()->joinThreadPool();

  LOG(INFO) << "Exiting perfprofd";
  return 0;
}

}  // namespace binder
}  // namespace perfprofd
}  // namespace android
