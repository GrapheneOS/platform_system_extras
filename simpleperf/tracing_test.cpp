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

TEST(tracing, check_tp_filter_format) {
  FieldNameSet used_fields;
  ASSERT_TRUE(CheckTracepointFilterFormat("((sig >= 1 && sig < 20) || sig == 32) && comm != \"bash\"",
                                          &used_fields));
  ASSERT_EQ(android::base::Join(used_fields, ";"), "comm;sig");
  ASSERT_TRUE(CheckTracepointFilterFormat("pid != 3 && !(comm ~ \"*bash\")", &used_fields));
  ASSERT_EQ(android::base::Join(used_fields, ";"), "comm;pid");
  ASSERT_TRUE(CheckTracepointFilterFormat("mask & 3", &used_fields));
  ASSERT_EQ(android::base::Join(used_fields, ";"), "mask");
  ASSERT_TRUE(CheckTracepointFilterFormat("addr > 0 && addr != 0xFFFFFFFFFFFFFFFF || value > -5",
                                          &used_fields));
  ASSERT_EQ(android::base::Join(used_fields, ";"), "addr;value");

  // unmatched paren
  ASSERT_FALSE(CheckTracepointFilterFormat("(pid > 3", &used_fields));
  ASSERT_FALSE(CheckTracepointFilterFormat("pid > 3)", &used_fields));
  // unknown operator
  ASSERT_FALSE(CheckTracepointFilterFormat("pid ^ 3", &used_fields));
  // field name not on the left
  ASSERT_FALSE(CheckTracepointFilterFormat("3 < pid", &used_fields));
  // no string quote
  ASSERT_FALSE(CheckTracepointFilterFormat("comm == sleep", &used_fields));
  // wrong int value
  ASSERT_FALSE(CheckTracepointFilterFormat("value > --5", &used_fields));
}
