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
"""test.py: Tests for simpleperf python scripts.

These are smoke tests Using examples to run python scripts.
For each example, we go through the steps of running each python script.
Examples are collected from simpleperf/demo, which includes:
  SimpleperfExamplePureJava
  SimpleperfExampleWithNative
  SimpleperfExampleOfKotlin

Tested python scripts include:
  app_profiler.py
  report.py
  annotate.py
  report_sample.py
  pprof_proto_generator.py
  report_html.py

Test using both `adb root` and `adb unroot`.

"""
import argparse
import fnmatch
import inspect
import re
from simpleperf_utils import extant_dir, log_exit, remove
import sys
import types
import unittest

from . api_profiler_test import *
from . app_profiler_test import *
from . app_test import *
from . binary_cache_builder_test import *
from . cpp_app_test import *
from . java_app_test import *
from . kotlin_app_test import *
from . pprof_proto_generator_test import *
from . report_html_test import *
from . report_lib_test import *
from . run_simpleperf_on_device_test import *
from . tools_test import *
from . test_utils import TEST_LOGGER, TEST_HELPER


def get_all_tests():
    tests = []
    for name, value in globals().items():
        if isinstance(value, type) and issubclass(value, unittest.TestCase):
            for member_name, member in inspect.getmembers(value):
                if isinstance(member, (types.MethodType, types.FunctionType)):
                    if member_name.startswith('test'):
                        tests.append(name + '.' + member_name)
    return sorted(tests)


def run_tests(tests):
    TEST_HELPER.build_testdata()
    argv = [sys.argv[0]] + tests
    test_runner = unittest.TextTestRunner(stream=TEST_LOGGER, verbosity=0)
    test_program = unittest.main(argv=argv, testRunner=test_runner,
                                 exit=False, verbosity=0, module='test.do_test')
    result = test_program.result.wasSuccessful()
    remove(TEST_HELPER.testdata_dir)
    return result


def main():
    parser = argparse.ArgumentParser(description='Test simpleperf scripts')
    parser.add_argument('--list-tests', action='store_true', help='List all tests.')
    parser.add_argument('--test-from', nargs=1, help='Run left tests from the selected test.')
    parser.add_argument('--browser', action='store_true', help='pop report html file in browser.')
    parser.add_argument('--progress-file', help='write test progress file')
    parser.add_argument('--ndk-path', type=extant_dir, help='Set the path of a ndk release')
    parser.add_argument('pattern', nargs='*', help='Run tests matching the selected pattern.')
    args = parser.parse_args()
    tests = get_all_tests()
    if args.list_tests:
        print('\n'.join(tests))
        return True
    if args.test_from:
        start_pos = 0
        while start_pos < len(tests) and tests[start_pos] != args.test_from[0]:
            start_pos += 1
        if start_pos == len(tests):
            log_exit("Can't find test %s" % args.test_from[0])
        tests = tests[start_pos:]
    if args.pattern:
        patterns = [re.compile(fnmatch.translate(x)) for x in args.pattern]
        tests = [t for t in tests if any(pattern.match(t) for pattern in patterns)]
        if not tests:
            log_exit('No tests are matched.')

    if TEST_HELPER.android_version < 7:
        print("Skip tests on Android version < N.", file=TEST_LOGGER)
        return False

    TEST_HELPER.ndk_path = args.ndk_path

    remove(TEST_HELPER.test_base_dir)

    if not args.browser:
        TEST_HELPER.browser_option = ['--no_browser']

    if args.progress_file:
        TEST_HELPER.progress_fh = open(args.progress_file, 'w')

    result = run_tests(tests)
    if not result:
        print('Tests failed, see %s for details.' % TEST_LOGGER.log_file, file=TEST_LOGGER)
    TEST_HELPER.write_progress('Test end')
    return result
