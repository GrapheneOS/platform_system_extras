#!/usr/bin/env python3
#
# Copyright (C) 2021 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import json

from . test_utils import TestBase, TestHelper


class TestGeckoProfileGenerator(TestBase):
    def run_generator(self, testdata_file):
        testdata_path = TestHelper.testdata_path(testdata_file)
        gecko_profile_json = self.run_cmd(
            ['gecko_profile_generator.py', '-i', testdata_path], return_output=True)
        return json.loads(gecko_profile_json)

    def test_golden(self):
        got = self.run_generator('perf_with_interpreter_frames.data')
        golden_path = TestHelper.testdata_path('perf_with_interpreter_frames.gecko.json')
        with open(golden_path) as f:
            want = json.load(f)
        self.assertEqual(
            json.dumps(got, sort_keys=True, indent=2),
            json.dumps(want, sort_keys=True, indent=2))
