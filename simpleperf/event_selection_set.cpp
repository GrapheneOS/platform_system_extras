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

#include "event_selection_set.h"

#include <poll.h>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "environment.h"
#include "event_attr.h"
#include "event_type.h"
#include "perf_regs.h"

bool IsBranchSamplingSupported() {
  const EventType* type = FindEventTypeByName("cpu-cycles");
  if (type == nullptr) {
    return false;
  }
  perf_event_attr attr = CreateDefaultPerfEventAttr(*type);
  attr.sample_type |= PERF_SAMPLE_BRANCH_STACK;
  attr.branch_sample_type = PERF_SAMPLE_BRANCH_ANY;
  return IsEventAttrSupportedByKernel(attr);
}

bool IsDwarfCallChainSamplingSupported() {
  const EventType* type = FindEventTypeByName("cpu-cycles");
  if (type == nullptr) {
    return false;
  }
  perf_event_attr attr = CreateDefaultPerfEventAttr(*type);
  attr.sample_type |=
      PERF_SAMPLE_CALLCHAIN | PERF_SAMPLE_REGS_USER | PERF_SAMPLE_STACK_USER;
  attr.exclude_callchain_user = 1;
  attr.sample_regs_user = GetSupportedRegMask(GetBuildArch());
  attr.sample_stack_user = 8192;
  return IsEventAttrSupportedByKernel(attr);
}

bool EventSelectionSet::BuildAndCheckEventSelection(
    const std::string& event_name, EventSelection* selection) {
  std::unique_ptr<EventTypeAndModifier> event_type = ParseEventType(event_name);
  if (event_type == nullptr) {
    return false;
  }
  selection->event_type_modifier = *event_type;
  selection->event_attr = CreateDefaultPerfEventAttr(event_type->event_type);
  selection->event_attr.exclude_user = event_type->exclude_user;
  selection->event_attr.exclude_kernel = event_type->exclude_kernel;
  selection->event_attr.exclude_hv = event_type->exclude_hv;
  selection->event_attr.exclude_host = event_type->exclude_host;
  selection->event_attr.exclude_guest = event_type->exclude_guest;
  selection->event_attr.precise_ip = event_type->precise_ip;
  if (!IsEventAttrSupportedByKernel(selection->event_attr)) {
    LOG(ERROR) << "Event type '" << event_type->name
               << "' is not supported by the kernel";
    return false;
  }
  selection->event_fds.clear();

  for (const auto& group : groups_) {
    for (const auto& sel : group) {
      if (sel.event_type_modifier.name == selection->event_type_modifier.name) {
        LOG(ERROR) << "Event type '" << sel.event_type_modifier.name
                   << "' appears more than once";
        return false;
      }
    }
  }
  return true;
}

bool EventSelectionSet::AddEventType(const std::string& event_name) {
  return AddEventGroup(std::vector<std::string>(1, event_name));
}

bool EventSelectionSet::AddEventGroup(
    const std::vector<std::string>& event_names) {
  EventSelectionGroup group;
  for (const auto& event_name : event_names) {
    EventSelection selection;
    if (!BuildAndCheckEventSelection(event_name, &selection)) {
      return false;
    }
    selection.selection_id = group.size();
    selection.group_id = groups_.size();
    group.push_back(std::move(selection));
  }
  groups_.push_back(std::move(group));
  UnionSampleType();
  return true;
}

// Union the sample type of different event attrs can make reading sample
// records in perf.data
// easier.
void EventSelectionSet::UnionSampleType() {
  uint64_t sample_type = 0;
  for (const auto& group : groups_) {
    for (const auto& selection : group) {
      sample_type |= selection.event_attr.sample_type;
    }
  }
  for (auto& group : groups_) {
    for (auto& selection : group) {
      selection.event_attr.sample_type = sample_type;
    }
  }
}

void EventSelectionSet::SetEnableOnExec(bool enable) {
  for (auto& group : groups_) {
    for (auto& selection : group) {
      // If sampling is enabled on exec, then it is disabled at startup,
      // otherwise
      // it should be enabled at startup. Don't use ioctl(PERF_EVENT_IOC_ENABLE)
      // to enable it after perf_event_open(). Because some android kernels
      // can't
      // handle ioctl() well when cpu-hotplug happens. See http://b/25193162.
      if (enable) {
        selection.event_attr.enable_on_exec = 1;
        selection.event_attr.disabled = 1;
      } else {
        selection.event_attr.enable_on_exec = 0;
        selection.event_attr.disabled = 0;
      }
    }
  }
}

bool EventSelectionSet::GetEnableOnExec() {
  for (const auto& group : groups_) {
    for (const auto& selection : group) {
      if (selection.event_attr.enable_on_exec == 0) {
        return false;
      }
    }
  }
  return true;
}

void EventSelectionSet::SampleIdAll() {
  for (auto& group : groups_) {
    for (auto& selection : group) {
      selection.event_attr.sample_id_all = 1;
    }
  }
}

void EventSelectionSet::SetSampleFreq(const EventSelection& selection,
                                      uint64_t sample_freq) {
  EventSelection& sel = groups_[selection.group_id][selection.selection_id];
  sel.event_attr.freq = 1;
  sel.event_attr.sample_freq = sample_freq;
}

void EventSelectionSet::SetSamplePeriod(const EventSelection& selection,
                                        uint64_t sample_period) {
  EventSelection& sel = groups_[selection.group_id][selection.selection_id];
  sel.event_attr.freq = 0;
  sel.event_attr.sample_period = sample_period;
}

bool EventSelectionSet::SetBranchSampling(uint64_t branch_sample_type) {
  if (branch_sample_type != 0 &&
      (branch_sample_type &
       (PERF_SAMPLE_BRANCH_ANY | PERF_SAMPLE_BRANCH_ANY_CALL |
        PERF_SAMPLE_BRANCH_ANY_RETURN | PERF_SAMPLE_BRANCH_IND_CALL)) == 0) {
    LOG(ERROR) << "Invalid branch_sample_type: 0x" << std::hex
               << branch_sample_type;
    return false;
  }
  if (branch_sample_type != 0 && !IsBranchSamplingSupported()) {
    LOG(ERROR) << "branch stack sampling is not supported on this device.";
    return false;
  }
  for (auto& group : groups_) {
    for (auto& selection : group) {
      perf_event_attr& attr = selection.event_attr;
      if (branch_sample_type != 0) {
        attr.sample_type |= PERF_SAMPLE_BRANCH_STACK;
      } else {
        attr.sample_type &= ~PERF_SAMPLE_BRANCH_STACK;
      }
      attr.branch_sample_type = branch_sample_type;
    }
  }
  return true;
}

void EventSelectionSet::EnableFpCallChainSampling() {
  for (auto& group : groups_) {
    for (auto& selection : group) {
      selection.event_attr.sample_type |= PERF_SAMPLE_CALLCHAIN;
    }
  }
}

bool EventSelectionSet::EnableDwarfCallChainSampling(uint32_t dump_stack_size) {
  if (!IsDwarfCallChainSamplingSupported()) {
    LOG(ERROR) << "dwarf callchain sampling is not supported on this device.";
    return false;
  }
  for (auto& group : groups_) {
    for (auto& selection : group) {
      selection.event_attr.sample_type |= PERF_SAMPLE_CALLCHAIN |
                                          PERF_SAMPLE_REGS_USER |
                                          PERF_SAMPLE_STACK_USER;
      selection.event_attr.exclude_callchain_user = 1;
      selection.event_attr.sample_regs_user =
          GetSupportedRegMask(GetBuildArch());
      selection.event_attr.sample_stack_user = dump_stack_size;
    }
  }
  return true;
}

void EventSelectionSet::SetInherit(bool enable) {
  for (auto& group : groups_) {
    for (auto& selection : group) {
      selection.event_attr.inherit = (enable ? 1 : 0);
    }
  }
}

static bool CheckIfCpusOnline(const std::vector<int>& cpus) {
  std::vector<int> online_cpus = GetOnlineCpus();
  for (const auto& cpu : cpus) {
    if (std::find(online_cpus.begin(), online_cpus.end(), cpu) ==
        online_cpus.end()) {
      LOG(ERROR) << "cpu " << cpu << " is not online.";
      return false;
    }
  }
  return true;
}

bool EventSelectionSet::OpenEventFilesForCpus(const std::vector<int>& cpus) {
  return OpenEventFilesForThreadsOnCpus({-1}, cpus);
}

bool EventSelectionSet::OpenEventFilesForThreadsOnCpus(
    const std::vector<pid_t>& threads, std::vector<int> cpus) {
  if (!cpus.empty()) {
    // cpus = {-1} means open an event file for all cpus.
    if (!(cpus.size() == 1 && cpus[0] == -1) && !CheckIfCpusOnline(cpus)) {
      return false;
    }
  } else {
    cpus = GetOnlineCpus();
  }
  return OpenEventFiles(threads, cpus);
}

bool EventSelectionSet::OpenEventFiles(const std::vector<pid_t>& threads,
                                       const std::vector<int>& cpus) {
  for (auto& group : groups_) {
    for (const auto& tid : threads) {
      size_t open_per_thread = 0;
      std::string failed_event_type;
      for (const auto& cpu : cpus) {
        std::vector<std::unique_ptr<EventFd>> event_fds;
        // Given a tid and cpu, events on the same group should be all opened
        // successfully or all failed to open.
        for (auto& selection : group) {
          EventFd* group_fd = nullptr;
          if (selection.selection_id != 0) {
            group_fd = event_fds[0].get();
          }
          std::unique_ptr<EventFd> event_fd =
              EventFd::OpenEventFile(selection.event_attr, tid, cpu, group_fd);
          if (event_fd != nullptr) {
            LOG(VERBOSE) << "OpenEventFile for " << event_fd->Name();
            event_fds.push_back(std::move(event_fd));
          } else {
            failed_event_type = selection.event_type_modifier.name;
            break;
          }
        }
        if (event_fds.size() == group.size()) {
          for (size_t i = 0; i < group.size(); ++i) {
            group[i].event_fds.push_back(std::move(event_fds[i]));
          }
          ++open_per_thread;
        }
      }
      // As the online cpus can be enabled or disabled at runtime, we may not
      // open event file for
      // all cpus successfully. But we should open at least one cpu
      // successfully.
      if (open_per_thread == 0) {
        PLOG(ERROR) << "failed to open perf event file for event_type "
                    << failed_event_type << " for "
                    << (tid == -1 ? "all threads" : android::base::StringPrintf(
                                                        " thread %d", tid));
        return false;
      }
    }
  }
  return true;
}

bool EventSelectionSet::ReadCounters(std::vector<CountersInfo>* counters) {
  counters->clear();
  for (auto& group : groups_) {
    for (auto& selection : group) {
      CountersInfo counters_info;
      counters_info.selection = &selection;
      for (auto& event_fd : selection.event_fds) {
        CountersInfo::CounterInfo counter_info;
        if (!event_fd->ReadCounter(&counter_info.counter)) {
          return false;
        }
        counter_info.tid = event_fd->ThreadId();
        counter_info.cpu = event_fd->Cpu();
        counters_info.counters.push_back(counter_info);
      }
      counters->push_back(counters_info);
    }
  }
  return true;
}

bool EventSelectionSet::MmapEventFiles(size_t min_mmap_pages,
                                       size_t max_mmap_pages,
                                       std::vector<pollfd>* pollfds) {
  for (size_t i = max_mmap_pages; i >= min_mmap_pages; i >>= 1) {
    if (MmapEventFiles(i, pollfds, i == min_mmap_pages)) {
      LOG(VERBOSE) << "Mapped buffer size is " << i << " pages.";
      return true;
    }
    for (auto& group : groups_) {
      for (auto& selection : group) {
        for (auto& event_fd : selection.event_fds) {
          event_fd->DestroyMappedBuffer();
        }
      }
    }
  }
  return false;
}

bool EventSelectionSet::MmapEventFiles(size_t mmap_pages,
                                       std::vector<pollfd>* pollfds,
                                       bool report_error) {
  pollfds->clear();
  for (auto& group : groups_) {
    for (auto& selection : group) {
      // For each event, allocate a mapped buffer for each cpu.
      std::map<int, EventFd*> cpu_map;
      for (auto& event_fd : selection.event_fds) {
        auto it = cpu_map.find(event_fd->Cpu());
        if (it != cpu_map.end()) {
          if (!event_fd->ShareMappedBuffer(*(it->second), report_error)) {
            return false;
          }
        } else {
          pollfd poll_fd;
          if (!event_fd->CreateMappedBuffer(mmap_pages, &poll_fd,
                                            report_error)) {
            return false;
          }
          pollfds->push_back(poll_fd);
          cpu_map.insert(std::make_pair(event_fd->Cpu(), event_fd.get()));
        }
      }
    }
  }
  return true;
}

void EventSelectionSet::PrepareToReadMmapEventData(
    std::function<bool(Record*)> callback) {
  record_callback_ = callback;
  bool has_timestamp = true;
  for (const auto& group : groups_) {
    for (const auto& selection : group) {
      if (!IsTimestampSupported(selection.event_attr)) {
        has_timestamp = false;
        break;
      }
    }
  }
  record_cache_.reset(new RecordCache(has_timestamp));

  for (const auto& group : groups_) {
    for (const auto& selection : group) {
      for (const auto& event_fd : selection.event_fds) {
        int event_id = event_fd->Id();
        event_id_to_attr_map_[event_id] = &selection.event_attr;
      }
    }
  }
}

bool EventSelectionSet::ReadMmapEventData() {
  for (auto& group : groups_) {
    for (auto& selection : group) {
      for (auto& event_fd : selection.event_fds) {
        bool has_data = true;
        while (has_data) {
          if (!ReadMmapEventDataForFd(event_fd, selection.event_attr,
                                      &has_data)) {
            return false;
          }
        }
      }
    }
  }
  return true;
}

bool EventSelectionSet::ReadMmapEventDataForFd(
    std::unique_ptr<EventFd>& event_fd, const perf_event_attr& attr,
    bool* has_data) {
  *has_data = false;
  while (true) {
    char* data;
    size_t size = event_fd->GetAvailableMmapData(&data);
    if (size == 0) {
      break;
    }
    std::vector<std::unique_ptr<Record>> records =
        ReadRecordsFromBuffer(attr, data, size);
    record_cache_->Push(std::move(records));
    std::unique_ptr<Record> r = record_cache_->Pop();
    while (r != nullptr) {
      if (!record_callback_(r.get())) {
        return false;
      }
      r = record_cache_->Pop();
    }
    *has_data = true;
  }
  return true;
}

bool EventSelectionSet::FinishReadMmapEventData() {
  std::vector<std::unique_ptr<Record>> records = record_cache_->PopAll();
  for (auto& r : records) {
    if (!record_callback_(r.get())) {
      return false;
    }
  }
  return true;
}
