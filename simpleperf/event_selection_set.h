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

#ifndef SIMPLE_PERF_EVENT_SELECTION_SET_H_
#define SIMPLE_PERF_EVENT_SELECTION_SET_H_

#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

#include <android-base/macros.h>

#include "event_fd.h"
#include "event_type.h"
#include "perf_event.h"
#include "record.h"

constexpr double DEFAULT_PERIOD_TO_DETECT_CPU_HOTPLUG_EVENTS_IN_SEC = 0.5;

struct EventSelection {
  uint32_t group_id;
  uint32_t selection_id;
  EventTypeAndModifier event_type_modifier;
  perf_event_attr event_attr;
  std::vector<std::unique_ptr<EventFd>> event_fds;
};
typedef std::vector<EventSelection> EventSelectionGroup;

struct CountersInfo {
  const EventSelection* selection;
  struct CounterInfo {
    pid_t tid;
    int cpu;
    PerfCounter counter;
  };
  std::vector<CounterInfo> counters;
};

class IOEventLoop;

// EventSelectionSet helps to monitor events. It is used in following steps:
// 1. Create an EventSelectionSet, and add event types to monitor by calling
//    AddEventType() or AddEventGroup().
// 2. Define how to monitor events by calling SetEnableOnExec(), SampleIdAll(),
//    SetSampleFreq(), etc.
// 3. Start monitoring by calling OpenEventFilesForCpus() or
//    OpenEventFilesForThreadsOnCpus(). If SetEnableOnExec() has been called
//    in step 2, monitor will be delayed until the monitored thread calls
//    exec().
// 4. Read counters by calling ReadCounters(), or read mapped event records
//    by calling MmapEventFiles(), PrepareToReadMmapEventData() and
//    FinishReadMmapEventData().
// 5. Stop monitoring automatically in the destructor of EventSelectionSet by
//    closing perf event files.

class EventSelectionSet {
 public:
  EventSelectionSet(bool for_stat_cmd) : for_stat_cmd_(for_stat_cmd) {}

  bool empty() const { return groups_.empty(); }

  const std::vector<EventSelectionGroup>& groups() { return groups_; }

  bool AddEventType(const std::string& event_name);
  bool AddEventGroup(const std::vector<std::string>& event_names);

  void SetEnableOnExec(bool enable);
  bool GetEnableOnExec();
  void SampleIdAll();
  void SetSampleFreq(const EventSelection& selection, uint64_t sample_freq);
  void SetSamplePeriod(const EventSelection& selection, uint64_t sample_period);
  bool SetBranchSampling(uint64_t branch_sample_type);
  void EnableFpCallChainSampling();
  bool EnableDwarfCallChainSampling(uint32_t dump_stack_size);
  void SetInherit(bool enable);
  void SetLowWatermark();

  bool OpenEventFilesForCpus(const std::vector<int>& cpus);
  bool OpenEventFilesForThreadsOnCpus(const std::vector<pid_t>& threads,
                                      std::vector<int> cpus);
  bool ReadCounters(std::vector<CountersInfo>* counters);
  bool MmapEventFiles(size_t min_mmap_pages, size_t max_mmap_pages);
  bool PrepareToReadMmapEventData(IOEventLoop& loop,
                                  const std::function<bool(Record*)>& callback);
  bool FinishReadMmapEventData();

  // If monitored_cpus is empty, monitor all cpus.
  bool HandleCpuHotplugEvents(
      IOEventLoop& loop, const std::vector<int>& monitored_cpus,
      double check_interval_in_sec =
          DEFAULT_PERIOD_TO_DETECT_CPU_HOTPLUG_EVENTS_IN_SEC);

 private:
  bool BuildAndCheckEventSelection(const std::string& event_name,
                                   EventSelection* selection);
  void UnionSampleType();
  bool OpenEventFiles(const std::vector<pid_t>& threads,
                      const std::vector<int>& cpus);
  bool MmapEventFiles(size_t mmap_pages, bool report_error);
  bool ReadMmapEventDataForFd(std::unique_ptr<EventFd>& event_fd);

  bool DetectCpuHotplugEvents();

  const bool for_stat_cmd_;

  std::vector<EventSelectionGroup> groups_;

  std::function<bool(Record*)> record_callback_;

  std::set<int> monitored_cpus_;
  std::vector<int> online_cpus_;

  DISALLOW_COPY_AND_ASSIGN(EventSelectionSet);
};

bool IsBranchSamplingSupported();
bool IsDwarfCallChainSamplingSupported();

#endif  // SIMPLE_PERF_EVENT_SELECTION_SET_H_
