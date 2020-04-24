/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <android-base/file.h>
#include <gtest/gtest.h>

#include "cmd_inject_impl.h"
#include "command.h"
#include "get_test_data.h"
#include "test_util.h"
#include "utils.h"

using namespace simpleperf;

static std::unique_ptr<Command> InjectCmd() { return CreateCommandInstance("inject"); }

static bool RunInjectCmd(std::vector<std::string>&& args) {
  bool has_input = std::find(args.begin(), args.end(), "-i") != args.end();
  if (!has_input) {
    args.insert(args.end(), {"-i", GetTestData(PERF_DATA_ETM_TEST_LOOP)});
  }
  args.insert(args.end(), {"--symdir", GetTestDataDir() + "etm"});
  return InjectCmd()->Run(args);
}

static bool RunInjectCmd(std::vector<std::string>&& args, std::string* output) {
  TemporaryFile tmpfile;
  args.insert(args.end(), {"-o", tmpfile.path});
  if (!RunInjectCmd(std::move(args))) {
    return false;
  }
  return android::base::ReadFileToString(tmpfile.path, output);
}

static void CheckMatchingExpectedData(std::string& data) {
  std::string expected_data;
  ASSERT_TRUE(android::base::ReadFileToString(
      GetTestData(std::string("etm") + OS_PATH_SEPARATOR + "perf_inject.data"), &expected_data));
  data.erase(std::remove(data.begin(), data.end(), '\r'), data.end());
  ASSERT_EQ(data, expected_data);
}

TEST(cmd_inject, smoke) {
  std::string data;
  ASSERT_TRUE(RunInjectCmd({}, &data));
  // Test that we can find instr range in etm_test_loop binary.
  ASSERT_NE(data.find("etm_test_loop"), std::string::npos);
  CheckMatchingExpectedData(data);
}

TEST(cmd_inject, binary_option) {
  // Test that data for etm_test_loop is generated when selected by --binary.
  std::string data;
  ASSERT_TRUE(RunInjectCmd({"--binary", "etm_test_loop"}, &data));
  ASSERT_NE(data.find("etm_test_loop"), std::string::npos);

  // Test that data for etm_test_loop is generated when selected by regex.
  ASSERT_TRUE(RunInjectCmd({"--binary", "etm_t.*_loop"}, &data));
  ASSERT_NE(data.find("etm_test_loop"), std::string::npos);

  // Test that data for etm_test_loop isn't generated when not selected by --binary.
  ASSERT_TRUE(RunInjectCmd({"--binary", "no_etm_test_loop"}, &data));
  ASSERT_EQ(data.find("etm_test_loop"), std::string::npos);

  // Test that data for etm_test_loop isn't generated when not selected by regex.
  ASSERT_TRUE(RunInjectCmd({"--binary", "no_etm_test_.*"}, &data));
  ASSERT_EQ(data.find("etm_test_loop"), std::string::npos);
}

TEST(cmd_inject, output_option) {
  TemporaryFile tmpfile;
  ASSERT_TRUE(RunInjectCmd({"--output", "autofdo", "-o", tmpfile.path}));
  ASSERT_TRUE(RunInjectCmd({"--output", "branch-list", "-o", tmpfile.path}));
  std::string autofdo_data;
  ASSERT_TRUE(RunInjectCmd({"-i", tmpfile.path, "--output", "autofdo"}, &autofdo_data));
  CheckMatchingExpectedData(autofdo_data);
}

TEST(cmd_inject, branch_to_proto_string) {
  std::vector<bool> branch;
  for (size_t i = 0; i < 100; i++) {
    branch.push_back(i % 2 == 0);
    std::string s = BranchToProtoString(branch);
    for (size_t j = 0; j <= i; j++) {
      bool b = s[j >> 3] & (1 << (j & 7));
      ASSERT_EQ(b, branch[j]);
    }
    std::vector<bool> branch2 = ProtoStringToBranch(s, branch.size());
    ASSERT_EQ(branch, branch2);
  }
}
