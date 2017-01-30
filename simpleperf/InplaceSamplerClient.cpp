/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "InplaceSamplerClient.h"

#include <sys/time.h>
#include <sys/types.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "environment.h"
#include "utils.h"

static constexpr uint64_t EVENT_ID_FOR_INPLACE_SAMPLER = ULONG_MAX;

std::unique_ptr<InplaceSamplerClient> InplaceSamplerClient::Create(const perf_event_attr& attr,
                                                                   pid_t pid,
                                                                   const std::set<pid_t>& tids) {
  if (pid == -1) {
    LOG(ERROR) << "inplace-sampler can't monitor system wide events.";
    return nullptr;
  }
  std::unique_ptr<InplaceSamplerClient> sampler(new InplaceSamplerClient(attr, pid, tids));
  if (!sampler->ConnectServer()) {
    return nullptr;
  }
  if (!sampler->StartProfiling()) {
    return nullptr;
  }
  return sampler;
}

InplaceSamplerClient::InplaceSamplerClient(const perf_event_attr& attr, pid_t pid,
                                           const std::set<pid_t>& tids)
    : attr_(attr), pid_(pid), tids_(tids), closed_(false) {
}

uint64_t InplaceSamplerClient::Id() const {
  return EVENT_ID_FOR_INPLACE_SAMPLER;
}

bool InplaceSamplerClient::ConnectServer() {
  return true;
}

bool InplaceSamplerClient::StartProfiling() {
  return true;
}

bool InplaceSamplerClient::StartPolling(IOEventLoop& loop,
                                        const std::function<bool(Record*)>& record_callback,
                                        const std::function<bool()>& close_callback) {
  record_callback_ = record_callback;
  close_callback_ = close_callback;
  auto callback = [this]() {
    // Fake records for testing.
    uint64_t time = GetSystemClock();
    CommRecord comm_r(attr_, pid_, pid_, "fake_comm", Id(), time);
    if (!record_callback_(&comm_r)) {
      return false;
    }
    MmapRecord mmap_r(attr_, false, pid_, pid_, 0x1000, 0x1000, 0x0, "fake_elf", Id(), time);
    if (!record_callback_(&mmap_r)) {
      return false;
    }
    std::vector<uint64_t> ips(1, 0x1000);
    SampleRecord r(attr_, Id(), ips[0], pid_, pid_, time, 0, 1, ips);
    if (!record_callback_(&r)) {
      return false;
    }
    closed_ = true;
    return close_callback_();
  };
  timeval duration;
  duration.tv_sec = 0;
  duration.tv_usec = 1000;
  return loop.AddPeriodicEvent(duration, callback);
}
