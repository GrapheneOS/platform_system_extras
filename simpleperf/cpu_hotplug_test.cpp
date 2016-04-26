/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include <sys/stat.h>
#include <unistd.h>
#if defined(__BIONIC__)
#include <sys/system_properties.h>
#endif

#include <atomic>
#include <chrono>
#include <thread>
#include <unordered_map>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "event_attr.h"
#include "event_fd.h"
#include "event_type.h"
#include "utils.h"

#if defined(__BIONIC__)
class ScopedMpdecisionKiller {
 public:
  ScopedMpdecisionKiller() {
    have_mpdecision_ = IsMpdecisionRunning();
    if (have_mpdecision_) {
      DisableMpdecision();
    }
  }

  ~ScopedMpdecisionKiller() {
    if (have_mpdecision_) {
      EnableMpdecision();
    }
  }

 private:
  bool IsMpdecisionRunning() {
    char value[PROP_VALUE_MAX];
    int len = __system_property_get("init.svc.mpdecision", value);
    if (len == 0 || (len > 0 && strstr(value, "stopped") != nullptr)) {
      return false;
    }
    return true;
  }

  void DisableMpdecision() {
    int ret = __system_property_set("ctl.stop", "mpdecision");
    CHECK_EQ(0, ret);
    // Need to wait until mpdecision is actually stopped.
    usleep(500000);
    CHECK(!IsMpdecisionRunning());
  }

  void EnableMpdecision() {
    int ret = __system_property_set("ctl.start", "mpdecision");
    CHECK_EQ(0, ret);
    usleep(500000);
    CHECK(IsMpdecisionRunning());
  }

  bool have_mpdecision_;
};
#else
class ScopedMpdecisionKiller {
 public:
  ScopedMpdecisionKiller() {
  }
};
#endif

static bool IsCpuOnline(int cpu, bool* has_error) {
  std::string filename = android::base::StringPrintf("/sys/devices/system/cpu/cpu%d/online", cpu);
  std::string content;
  bool ret = android::base::ReadFileToString(filename, &content);
  if (!ret) {
    PLOG(ERROR) << "failed to read file " << filename;
    *has_error = true;
    return false;
  }
  *has_error = false;
  return (content.find('1') != std::string::npos);
}

static bool SetCpuOnline(int cpu, bool online) {
  bool has_error;
  bool ret = IsCpuOnline(cpu, &has_error);
  if (has_error) {
    return false;
  }
  if (ret == online) {
    return true;
  }
  std::string filename = android::base::StringPrintf("/sys/devices/system/cpu/cpu%d/online", cpu);
  std::string content = online ? "1" : "0";
  ret = android::base::WriteStringToFile(content, filename);
  if (!ret) {
    ret = IsCpuOnline(cpu, &has_error);
    if (has_error) {
      return false;
    }
    if (online == ret) {
      return true;
    }
    PLOG(ERROR) << "failed to write " << content << " to " << filename;
    return false;
  }
  // Kernel needs time to offline/online cpus, so use a loop to wait here.
  size_t retry_count = 0;
  while (true) {
    ret = IsCpuOnline(cpu, &has_error);
    if (has_error) {
      return false;
    }
    if (ret == online) {
      break;
    }
    LOG(ERROR) << "reading cpu retry count = " << retry_count << ", requested = " << online
        << ", real = " << ret;
    if (++retry_count == 10000) {
      LOG(ERROR) << "setting cpu " << cpu << (online ? " online" : " offline") << " seems not to take effect";
      return false;
    }
    usleep(1000);
  }
  return true;
}

static int GetCpuCount() {
  return static_cast<int>(sysconf(_SC_NPROCESSORS_CONF));
}

class CpuOnlineRestorer {
 public:
  CpuOnlineRestorer() {
    for (int cpu = 1; cpu < GetCpuCount(); ++cpu) {
      bool has_error;
      bool ret = IsCpuOnline(cpu, &has_error);
      if (has_error) {
        continue;
      }
      online_map_[cpu] = ret;
    }
  }

  ~CpuOnlineRestorer() {
    for (const auto& pair : online_map_) {
      SetCpuOnline(pair.first, pair.second);
    }
  }

 private:
  std::unordered_map<int, bool> online_map_;
};

bool FindAHotpluggableCpu(int* hotpluggable_cpu) {
  if (!IsRoot()) {
    GTEST_LOG_(INFO) << "This test needs root privilege to hotplug cpu.";
    return false;
  }
  for (int cpu = 1; cpu < GetCpuCount(); ++cpu) {
    bool has_error;
    bool online = IsCpuOnline(cpu, &has_error);
    if (has_error) {
      continue;
    }
    if (SetCpuOnline(cpu, !online)) {
      *hotpluggable_cpu = cpu;
      return true;
    }
  }
  GTEST_LOG_(INFO) << "There is no hotpluggable cpu.";
  return false;
}

struct CpuToggleThreadArg {
  int toggle_cpu;
  std::atomic<bool> end_flag;
};

static void CpuToggleThread(CpuToggleThreadArg* arg) {
  while (!arg->end_flag) {
    CHECK(SetCpuOnline(arg->toggle_cpu, true));
    CHECK(SetCpuOnline(arg->toggle_cpu, false));
  }
}

// http://b/25193162.
TEST(cpu_offline, offline_while_recording) {
  ScopedMpdecisionKiller scoped_mpdecision_killer;
  CpuOnlineRestorer cpuonline_restorer;
  if (GetCpuCount() == 1) {
    GTEST_LOG_(INFO) << "This test does nothing, because there is only one cpu in the system.";
    return;
  }
  // Start cpu hotpluger.
  int test_cpu;
  if (!FindAHotpluggableCpu(&test_cpu)) {
    return;
  }
  CpuToggleThreadArg cpu_toggle_arg;
  cpu_toggle_arg.toggle_cpu = test_cpu;
  cpu_toggle_arg.end_flag = false;
  std::thread cpu_toggle_thread(CpuToggleThread, &cpu_toggle_arg);

  std::unique_ptr<EventTypeAndModifier> event_type_modifier = ParseEventType("cpu-cycles");
  ASSERT_TRUE(event_type_modifier != nullptr);
  perf_event_attr attr = CreateDefaultPerfEventAttr(event_type_modifier->event_type);
  attr.disabled = 0;
  attr.enable_on_exec = 0;

  const std::chrono::minutes test_duration(2);  // Test for 2 minutes.
  auto end_time = std::chrono::steady_clock::now() + test_duration;
  size_t iterations = 0;

  while (std::chrono::steady_clock::now() < end_time) {
    std::unique_ptr<EventFd> event_fd = EventFd::OpenEventFile(attr, -1, test_cpu, false);
    if (event_fd == nullptr) {
      // Failed to open because the test_cpu is offline.
      continue;
    }
    iterations++;
    GTEST_LOG_(INFO) << "Test offline while recording for " << iterations << " times.";
  }
  cpu_toggle_arg.end_flag = true;
  cpu_toggle_thread.join();
}

// http://b/25193162.
TEST(cpu_offline, offline_while_ioctl_enable) {
  ScopedMpdecisionKiller scoped_mpdecision_killer;
  CpuOnlineRestorer cpuonline_restorer;
  if (GetCpuCount() == 1) {
    GTEST_LOG_(INFO) << "This test does nothing, because there is only one cpu in the system.";
    return;
  }
  // Start cpu hotpluger.
  int test_cpu;
  if (!FindAHotpluggableCpu(&test_cpu)) {
    return;
  }
  CpuToggleThreadArg cpu_toggle_arg;
  cpu_toggle_arg.toggle_cpu = test_cpu;
  cpu_toggle_arg.end_flag = false;
  std::thread cpu_toggle_thread(CpuToggleThread, &cpu_toggle_arg);

  std::unique_ptr<EventTypeAndModifier> event_type_modifier = ParseEventType("cpu-cycles");
  ASSERT_TRUE(event_type_modifier != nullptr);
  perf_event_attr attr = CreateDefaultPerfEventAttr(event_type_modifier->event_type);
  attr.disabled = 1;
  attr.enable_on_exec = 0;

  const std::chrono::minutes test_duration(2);  // Test for 2 minutes.
  auto end_time = std::chrono::steady_clock::now() + test_duration;
  size_t iterations = 0;

  while (std::chrono::steady_clock::now() < end_time) {
    std::unique_ptr<EventFd> event_fd = EventFd::OpenEventFile(attr, -1, test_cpu, false);
    if (event_fd == nullptr) {
      // Failed to open because the test_cpu is offline.
      continue;
    }
    // Wait a little for the event to be installed on test_cpu's perf context.
    usleep(1000);
    ASSERT_TRUE(event_fd->EnableEvent());
    iterations++;
    GTEST_LOG_(INFO) << "Test offline while ioctl(PERF_EVENT_IOC_ENABLE) for " << iterations << " times.";
  }
  cpu_toggle_arg.end_flag = true;
  cpu_toggle_thread.join();
}

// http://b/19863147.
TEST(cpu_offline, offline_while_recording_on_another_cpu) {
  ScopedMpdecisionKiller scoped_mpdecision_killer;
  CpuOnlineRestorer cpuonline_restorer;

  if (GetCpuCount() == 1) {
    GTEST_LOG_(INFO) << "This test does nothing, because there is only one cpu in the system.";
    return;
  }
  int test_cpu;
  if (!FindAHotpluggableCpu(&test_cpu)) {
    return;
  }
  std::unique_ptr<EventTypeAndModifier> event_type_modifier = ParseEventType("cpu-cycles");
  perf_event_attr attr = CreateDefaultPerfEventAttr(event_type_modifier->event_type);
  attr.disabled = 0;
  attr.enable_on_exec = 0;

  const size_t TEST_ITERATION_COUNT = 10u;
  for (size_t i = 0; i < TEST_ITERATION_COUNT; ++i) {
    int record_cpu = 0;
    ASSERT_TRUE(SetCpuOnline(test_cpu, true));
    std::unique_ptr<EventFd> event_fd = EventFd::OpenEventFile(attr, getpid(), record_cpu);
    ASSERT_TRUE(event_fd != nullptr);
    ASSERT_TRUE(SetCpuOnline(test_cpu, false));
    event_fd = nullptr;
    event_fd = EventFd::OpenEventFile(attr, getpid(), record_cpu);
    ASSERT_TRUE(event_fd != nullptr);
  }
}

int main(int argc, char** argv) {
  InitLogging(argv, android::base::StderrLogger);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
