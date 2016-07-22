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

#include <android-base/file.h>
#include <android-base/stringprintf.h>
#include <android-base/test_utils.h>

#include "command.h"
#include "get_test_data.h"
#include "test_util.h"

static std::unique_ptr<Command> StatCmd() {
  return CreateCommandInstance("stat");
}

TEST(stat_cmd, no_options) { ASSERT_TRUE(StatCmd()->Run({"sleep", "1"})); }

TEST(stat_cmd, event_option) {
  ASSERT_TRUE(StatCmd()->Run({"-e", "cpu-clock,task-clock", "sleep", "1"}));
}

TEST(stat_cmd, system_wide_option) {
  TEST_IN_ROOT(ASSERT_TRUE(StatCmd()->Run({"-a", "sleep", "1"})));
}

TEST(stat_cmd, verbose_option) {
  ASSERT_TRUE(StatCmd()->Run({"--verbose", "sleep", "1"}));
}

TEST(stat_cmd, tracepoint_event) {
  TEST_IN_ROOT(ASSERT_TRUE(
      StatCmd()->Run({"-a", "-e", "sched:sched_switch", "sleep", "1"})));
}

TEST(stat_cmd, event_modifier) {
  ASSERT_TRUE(
      StatCmd()->Run({"-e", "cpu-cycles:u,cpu-cycles:k", "sleep", "1"}));
}

void CreateProcesses(size_t count,
                     std::vector<std::unique_ptr<Workload>>* workloads) {
  workloads->clear();
  for (size_t i = 0; i < count; ++i) {
    // Create a workload runs longer than profiling time.
    auto workload = Workload::CreateWorkload({"sleep", "1000"});
    ASSERT_TRUE(workload != nullptr);
    ASSERT_TRUE(workload->Start());
    workloads->push_back(std::move(workload));
  }
}

TEST(stat_cmd, existing_processes) {
  std::vector<std::unique_ptr<Workload>> workloads;
  CreateProcesses(2, &workloads);
  std::string pid_list = android::base::StringPrintf(
      "%d,%d", workloads[0]->GetPid(), workloads[1]->GetPid());
  ASSERT_TRUE(StatCmd()->Run({"-p", pid_list, "sleep", "1"}));
}

TEST(stat_cmd, existing_threads) {
  std::vector<std::unique_ptr<Workload>> workloads;
  CreateProcesses(2, &workloads);
  // Process id can be used as thread id in linux.
  std::string tid_list = android::base::StringPrintf(
      "%d,%d", workloads[0]->GetPid(), workloads[1]->GetPid());
  ASSERT_TRUE(StatCmd()->Run({"-t", tid_list, "sleep", "1"}));
}

TEST(stat_cmd, no_monitored_threads) { ASSERT_FALSE(StatCmd()->Run({""})); }

TEST(stat_cmd, cpu_option) {
  ASSERT_TRUE(StatCmd()->Run({"--cpu", "0", "sleep", "1"}));
  TEST_IN_ROOT(ASSERT_TRUE(StatCmd()->Run({"--cpu", "0", "-a", "sleep", "1"})));
}

TEST(stat_cmd, group_option) {
  ASSERT_TRUE(
      StatCmd()->Run({"--group", "cpu-cycles,cpu-clock", "sleep", "1"}));
  ASSERT_TRUE(StatCmd()->Run({"--group", "cpu-cycles,cpu-clock", "--group",
                              "cpu-cycles:u,cpu-clock:u", "--group",
                              "cpu-cycles:k,cpu-clock:k", "sleep", "1"}));
}

TEST(stat_cmd, auto_generated_summary) {
  TemporaryFile tmp_file;
  ASSERT_TRUE(StatCmd()->Run({"--group", "cpu-clock:u,cpu-clock:k", "-o",
                              tmp_file.path, "sleep", "1"}));
  std::string s;
  ASSERT_TRUE(android::base::ReadFileToString(tmp_file.path, &s));
  size_t pos = s.find("cpu-clock:u");
  ASSERT_NE(s.npos, pos);
  pos = s.find("cpu-clock:k", pos);
  ASSERT_NE(s.npos, pos);
  pos += strlen("cpu-clock:k");
  // Check if the summary of cpu-clock is generated.
  ASSERT_NE(s.npos, s.find("cpu-clock", pos));
}

TEST(stat_cmd, duration_option) {
  ASSERT_TRUE(StatCmd()->Run({"--duration", "1.2"}));
}
