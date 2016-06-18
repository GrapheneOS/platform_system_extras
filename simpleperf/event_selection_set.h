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
#include <unordered_map>
#include <vector>

#include <android-base/macros.h>

#include "event_fd.h"
#include "event_type.h"
#include "perf_event.h"
#include "record.h"

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

struct pollfd;

// EventSelectionSet helps to monitor events.
// Firstly, the user creates an EventSelectionSet, and adds the specific event types to monitor.
// Secondly, the user defines how to monitor the events (by setting enable_on_exec flag,
// sample frequency, etc).
// Then, the user can start monitoring by ordering the EventSelectionSet to open perf event files
// and enable events (if enable_on_exec flag isn't used).
// After that, the user can read counters or read mapped event records.
// At last, the EventSelectionSet will clean up resources at destruction automatically.

class EventSelectionSet {
 public:
  EventSelectionSet() {
  }

  bool empty() const {
    return groups_.empty();
  }

  const std::vector<EventSelectionGroup>& groups() {
    return groups_;
  }

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

  bool OpenEventFilesForCpus(const std::vector<int>& cpus);
  bool OpenEventFilesForThreadsOnCpus(const std::vector<pid_t>& threads, std::vector<int> cpus);
  bool ReadCounters(std::vector<CountersInfo>* counters);
  void PrepareToPollForEventFiles(std::vector<pollfd>* pollfds);
  bool MmapEventFiles(size_t mmap_pages);
  void PrepareToReadMmapEventData(std::function<bool (Record*)> callback);
  bool ReadMmapEventData();
  bool FinishReadMmapEventData();

 private:
  bool BuildAndCheckEventSelection(const std::string& event_name,
                                   EventSelection* selection);
  void UnionSampleType();
  bool OpenEventFiles(const std::vector<pid_t>& threads, const std::vector<int>& cpus);
  bool ReadMmapEventDataForFd(std::unique_ptr<EventFd>& event_fd, const perf_event_attr& attr,
                              bool* has_data);

  std::vector<EventSelectionGroup> groups_;

  std::function<bool (Record*)> record_callback_;
  std::unique_ptr<RecordCache> record_cache_;
  std::unordered_map<uint64_t, const perf_event_attr*> event_id_to_attr_map_;

  DISALLOW_COPY_AND_ASSIGN(EventSelectionSet);
};

bool IsBranchSamplingSupported();
bool IsDwarfCallChainSamplingSupported();

#endif  // SIMPLE_PERF_EVENT_SELECTION_SET_H_
