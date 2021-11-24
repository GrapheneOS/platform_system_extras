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

from . test_utils import TestBase, TestHelper


class TestReportSample(TestBase):

    def test_no_flags(self):
        got = self.run_cmd(
            ['report_sample.py',
             '-i',
             TestHelper.testdata_path('perf_display_bitmaps.data')],
            return_output=True)
        with open(TestHelper.testdata_path('perf_display_bitmaps.perf-script')) as f:
            want = f.read()
        self.assertEqual(got, want)

    def test_comm_filter_to_renderthread(self):
        got = self.run_cmd(
            ['report_sample.py',
             '-i',
             TestHelper.testdata_path('perf_display_bitmaps.data'),
             '--comm', 'RenderThread'],
            return_output=True)
        self.assertIn('RenderThread', got)
        self.assertNotIn('com.example.android.displayingbitmaps', got)

        with open(TestHelper.testdata_path('perf_display_bitmaps.RenderThread.perf-script')) as f:
            want = f.read()
        self.assertEqual(got, want)

    def test_comm_filter_to_ui_thread(self):
        got = self.run_cmd(
            ['report_sample.py',
             '-i',
             TestHelper.testdata_path('perf_display_bitmaps.data'),
             '--comm', 'com.example.android.displayingbitmaps'],
            return_output=True)
        self.assertIn('com.example.android.displayingbitmaps', got)
        self.assertNotIn('RenderThread', got)
        with open(TestHelper.testdata_path('perf_display_bitmaps.UiThread.perf-script')) as f:
            want = f.read()
        self.assertEqual(got, want)

    def test_header(self):
        got = self.run_cmd(
            ['report_sample.py',
             '-i',
             TestHelper.testdata_path('perf_display_bitmaps.data'),
             '--header'],
            return_output=True)
        with open(TestHelper.testdata_path('perf_display_bitmaps.header.perf-script')) as f:
            want = f.read()
        self.assertEqual(got, want)

    def test_trace_offcpu(self):
        got = self.run_cmd(
            ['report_sample.py', '-i', TestHelper.testdata_path('perf_with_trace_offcpu_v2.data'),
             '--trace-offcpu', 'on-cpu'], return_output=True)
        self.assertIn('cpu-clock:u', got)
        self.assertNotIn('sched:sched_switch', got)
