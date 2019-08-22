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

#include "command.h"
#include "get_test_data.h"
#include "test_util.h"

static std::unique_ptr<Command> InjectCmd() { return CreateCommandInstance("inject"); }

TEST(cmd_inject, smoke) {
  TemporaryFile tmpfile;
  ASSERT_TRUE(InjectCmd()->Run({"--symdir", GetTestDataDir() + "etm", "-i",
                                GetTestData(PERF_DATA_ETM_TEST_LOOP), "-o", tmpfile.path}));
  std::string data;
  ASSERT_TRUE(android::base::ReadFileToString(tmpfile.path, &data));
  // Test that we can find instr range in etm_test_loop binary.
  ASSERT_NE(data.find("etm_test_loop"), std::string::npos);
}
