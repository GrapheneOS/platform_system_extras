/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include <android-base/file.h>
#include <android-base/test_utils.h>

#include "command.h"
#include "get_test_data.h"
#include "test_util.h"

#if defined(__aarch64__)
static std::unique_ptr<Command> DebugUnwindCmd() {
  return CreateCommandInstance("debug-unwind");
}

class CaptureStdout {
 public:
  CaptureStdout() : started_(false) {}

  ~CaptureStdout() {
    if (started_) {
      Finish();
    }
  }

  bool Start() {
    old_stdout_ = dup(STDOUT_FILENO);
    if (old_stdout_ == -1) {
      return false;
    }
    started_ = true;
    tmpfile_.reset(new TemporaryFile);
    if (dup2(tmpfile_->fd, STDOUT_FILENO) == -1) {
      return false;
    }
    return true;
  }

  std::string Finish() {
    started_ = false;
    dup2(old_stdout_, STDOUT_FILENO);
    close(old_stdout_);
    std::string s;
    if (!android::base::ReadFileToString(tmpfile_->path, &s)) {
      return "";
    }
    return s;
  }

 private:
  bool started_;
  int old_stdout_;
  std::unique_ptr<TemporaryFile> tmpfile_;
};
#endif  // defined(__aarch64__)

TEST(cmd_debug_unwind, smoke) {
  // TODO: Remove the arch limitation once using cross-platform unwinding in the new unwinder.
#if defined(__aarch64__)
  std::string input_data = GetTestData(PERF_DATA_NO_UNWIND);
  CaptureStdout capture;
  TemporaryFile tmp_file;
  ASSERT_TRUE(capture.Start());
  ASSERT_TRUE(DebugUnwindCmd()->Run({"-i", input_data, "-o", tmp_file.path}));
  ASSERT_NE(capture.Finish().find("Unwinding sample count: 8"), std::string::npos);

  ASSERT_TRUE(capture.Start());
  ASSERT_TRUE(DebugUnwindCmd()->Run({"-i", input_data, "-o", tmp_file.path, "--time",
                                     "1516379654300997"}));
  ASSERT_NE(capture.Finish().find("Unwinding sample count: 1"), std::string::npos);
#else
  GTEST_LOG_(INFO) << "This test does nothing on non-ARM64 devices.";
#endif
}
