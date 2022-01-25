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

import os
import re
import tempfile
from typing import Set

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

    def test_sample_filters(self):
        def get_threads_for_filter(filter: str) -> Set[int]:
            report = self.run_cmd(
                ['report_sample.py', '-i', TestHelper.testdata_path('perf_display_bitmaps.data')] +
                filter.split(), return_output=True)
            pattern = re.compile(r'\s+31850/(\d+)\s+')
            threads = set()
            for m in re.finditer(pattern, report):
                threads.add(int(m.group(1)))
            return threads

        self.assertNotIn(31850, get_threads_for_filter('--exclude-pid 31850'))
        self.assertIn(31850, get_threads_for_filter('--include-pid 31850'))
        self.assertIn(31850, get_threads_for_filter('--pid 31850'))
        self.assertNotIn(31881, get_threads_for_filter('--exclude-tid 31881'))
        self.assertIn(31881, get_threads_for_filter('--include-tid 31881'))
        self.assertIn(31881, get_threads_for_filter('--tid 31881'))
        self.assertNotIn(31881, get_threads_for_filter(
            '--exclude-process-name com.example.android.displayingbitmaps'))
        self.assertIn(31881, get_threads_for_filter(
            '--include-process-name com.example.android.displayingbitmaps'))
        self.assertNotIn(31850, get_threads_for_filter(
            '--exclude-thread-name com.example.android.displayingbitmaps'))
        self.assertIn(31850, get_threads_for_filter(
            '--include-thread-name com.example.android.displayingbitmaps'))

        with tempfile.NamedTemporaryFile('w', delete=False) as filter_file:
            filter_file.write('GLOBAL_BEGIN 684943449406175\nGLOBAL_END 684943449406176')
            filter_file.flush()
            threads = get_threads_for_filter('--filter-file ' + filter_file.name)
            self.assertIn(31881, threads)
            self.assertNotIn(31850, threads)
        os.unlink(filter_file.name)
