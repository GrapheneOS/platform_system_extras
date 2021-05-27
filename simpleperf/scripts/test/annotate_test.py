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

from pathlib import Path

from binary_cache_builder import BinaryCacheBuilder
from . test_utils import TestBase, TestHelper


class TestAnnotate(TestBase):
    def test_annotate(self):
        testdata_file = TestHelper.testdata_path('runtest_two_functions_arm64_perf.data')

        # Build binary_cache.
        binary_cache_builder = BinaryCacheBuilder(TestHelper.ndk_path, False)
        binary_cache_builder.build_binary_cache(testdata_file, [TestHelper.testdata_dir])

        # Generate annotated files.
        source_dir = TestHelper.testdata_dir
        self.run_cmd(['annotate.py', '-i', testdata_file, '-s', str(source_dir)])

        # Check annotated files.
        annotate_dir = Path('annotated_files')
        summary_file = annotate_dir / 'summary'
        check_items = [
            'two_functions.cpp: accumulated_period: 100.000000%, period: 100.000000%',
            'function (main): line 20, accumulated_period: 100.000000%, period: 0.000000%',
            'line 16: accumulated_period: 50.058937%, period: 50.058937%',
        ]
        self.check_strings_in_file(summary_file, check_items)

        source_files = list(annotate_dir.glob('**/*.cpp'))
        self.assertEqual(len(source_files), 1)
        source_file = source_files[0]
        self.assertEqual(source_file.name, 'two_functions.cpp')
        check_items = ['/* acc_p: 49.941063%, p: 49.941063%          */    *p = i;']
        self.check_strings_in_file(source_file, check_items)
