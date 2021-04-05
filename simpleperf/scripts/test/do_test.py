#!/usr/bin/env python3
#
# Copyright (C) 2017 The Android Open Source Project
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
#
"""Release test for simpleperf prebuilts.

It includes below tests:
1. Test profiling Android apps on different Android versions (starting from Android N).
2. Test simpleperf python scripts on different Hosts (linux, darwin and windows) on x86_64.
3. Test using both devices and emulators.
4. Test using both `adb root` and `adb unroot`.

"""

import argparse
import fnmatch
import inspect
import os
from pathlib import Path
import re
import sys
import time
import types
from typing import List, Optional
import unittest

from simpleperf_utils import extant_dir, log_exit, remove, ArgParseFormatter

from . api_profiler_test import *
from . app_profiler_test import *
from . app_test import *
from . binary_cache_builder_test import *
from . cpp_app_test import *
from . debug_unwind_reporter_test import *
from . java_app_test import *
from . kotlin_app_test import *
from . pprof_proto_generator_test import *
from . report_html_test import *
from . report_lib_test import *
from . run_simpleperf_on_device_test import *
from . tools_test import *
from . test_utils import TestHelper


def get_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=ArgParseFormatter)
    parser.add_argument('--browser', action='store_true', help='open report html file in browser.')
    parser.add_argument('--list-tests', action='store_true', help='List all tests.')
    parser.add_argument('--ndk-path', type=extant_dir, help='Set the path of a ndk release')
    parser.add_argument('-p', '--pattern', nargs='+',
                        help='Run tests matching the selected pattern.')
    parser.add_argument('--test-from', help='Run tests following the selected test.')
    parser.add_argument('--test-dir', default='test_dir', help='Directory to store test results')
    return parser.parse_args()


def get_all_tests() -> List[str]:
    tests = []
    for name, value in globals().items():
        if isinstance(value, type) and issubclass(value, unittest.TestCase):
            for member_name, member in inspect.getmembers(value):
                if isinstance(member, (types.MethodType, types.FunctionType)):
                    if member_name.startswith('test'):
                        tests.append(name + '.' + member_name)
    return sorted(tests)


def get_filtered_tests(test_from: Optional[str], test_pattern: Optional[List[str]]) -> List[str]:
    tests = get_all_tests()
    if test_from:
        try:
            tests = tests[tests.index(test_from):]
        except ValueError:
            log_exit("Can't find test %s" % test_from)
    if test_pattern:
        patterns = [re.compile(fnmatch.translate(x)) for x in test_pattern]
        tests = [t for t in tests if any(pattern.match(t) for pattern in patterns)]
        if not tests:
            log_exit('No tests are matched.')
    return tests


def build_testdata(testdata_dir: Path):
    """ Collect testdata in testdata_dir.
        In system/extras/simpleperf/scripts, testdata comes from:
            <script_dir>/../testdata, <script_dir>/test/script_testdata, <script_dir>/../demo
        In prebuilts/simpleperf, testdata comes from:
            <script_dir>/test/testdata
    """
    testdata_dir.mkdir()

    script_test_dir = Path(__file__).resolve().parent
    script_dir = script_test_dir.parent

    source_dirs = [
        script_test_dir / 'script_testdata',
        script_test_dir / 'testdata',
        script_dir.parent / 'testdata',
        script_dir.parent / 'demo',
    ]

    for source_dir in source_dirs:
        if not source_dir.is_dir():
            continue
        for src_path in source_dir.iterdir():
            dest_path = testdata_dir / src_path.name
            if dest_path.exists():
                continue
            if src_path.is_file():
                shutil.copyfile(src_path, dest_path)
            elif src_path.is_dir():
                shutil.copytree(src_path, dest_path)


def run_tests(tests: List[str]) -> bool:
    argv = [sys.argv[0]] + tests
    test_runner = unittest.TextTestRunner(stream=TestHelper.log_fh, verbosity=0)
    test_program = unittest.main(argv=argv, testRunner=test_runner,
                                 exit=False, verbosity=0, module='test.do_test')
    return test_program.result.wasSuccessful()


def main() -> bool:
    args = get_args()
    if args.list_tests:
        print('\n'.join(get_all_tests()))
        return True

    tests = get_filtered_tests(args.test_from, args.pattern)

    test_dir = Path(args.test_dir).resolve()
    remove(test_dir)
    test_dir.mkdir(parents=True)
    # Switch to the test dir.
    os.chdir(test_dir)
    build_testdata(Path('testdata'))

    TestHelper.init('.', 'testdata', args.browser, args.ndk_path, None, None)
    return run_tests(tests)
