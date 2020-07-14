/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "tracing.h"

#include <gtest/gtest.h>

#include <android-base/strings.h>

static void CheckAdjustFilter(const std::string& filter, bool use_quote,
                              const std::string& adjusted_filter,
                              const std::string used_field_str) {
  FieldNameSet used_fields;
  auto value = AdjustTracepointFilter(filter, use_quote, &used_fields);
  ASSERT_TRUE(value.has_value());
  ASSERT_EQ(value.value(), adjusted_filter);
  ASSERT_EQ(android::base::Join(used_fields, ","), used_field_str);
}

TEST(tracing, adjust_tracepoint_filter) {
  std::string filter = "((sig >= 1 && sig < 20) || sig == 32) && comm != \"bash\"";
  CheckAdjustFilter(filter, true, filter, "comm,sig");
  CheckAdjustFilter(filter, false, "((sig >= 1 && sig < 20) || sig == 32) && comm != bash",
                    "comm,sig");

  filter = "pid != 3 && !(comm ~ *bash)";
  CheckAdjustFilter(filter, true, "pid != 3 && !(comm ~ \"*bash\")", "comm,pid");
  CheckAdjustFilter(filter, false, filter, "comm,pid");

  filter = "mask & 3";
  CheckAdjustFilter(filter, true, filter, "mask");
  CheckAdjustFilter(filter, false, filter, "mask");

  filter = "addr > 0 && addr != 0xFFFFFFFFFFFFFFFF || value > -5";
  CheckAdjustFilter(filter, true, filter, "addr,value");
  CheckAdjustFilter(filter, false, filter, "addr,value");

  // unmatched paren
  FieldNameSet used_fields;
  ASSERT_FALSE(AdjustTracepointFilter("(pid > 3", true, &used_fields).has_value());
  ASSERT_FALSE(AdjustTracepointFilter("pid > 3)", true, &used_fields).has_value());
  // unknown operator
  ASSERT_FALSE(AdjustTracepointFilter("pid ^ 3", true, &used_fields).has_value());
}
