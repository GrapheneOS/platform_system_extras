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
#
"""test_utils.py: utils for testing.
"""

import logging
import os
from pathlib import Path
import shutil
import sys
from simpleperf_utils import remove, get_script_dir, AdbHelper, is_windows, bytes_to_str
import subprocess
import time
import unittest

INFERNO_SCRIPT = str(Path(__file__).parents[1] / ('inferno.bat' if is_windows() else 'inferno.sh'))


class TestLogger:
    """ Write test progress in sys.stderr and keep verbose log in log file. """

    def __init__(self):
        self.log_file = 'test.log'
        remove(self.log_file)
        # Logs can come from multiple processes. So use append mode to avoid overwrite.
        self.log_fh = open(self.log_file, 'a')
        logging.basicConfig(filename=self.log_file)

    def writeln(self, s):
        return self.write(s + '\n')

    def write(self, s):
        sys.stderr.write(s)
        self.log_fh.write(s)
        # Child processes can also write to log file, so flush it immediately to keep the order.
        self.flush()

    def flush(self):
        self.log_fh.flush()


TEST_LOGGER = TestLogger()


class TestHelper:
    """ Keep global test info. """

    def __init__(self):
        #self.script_dir = os.path.abspath(get_script_dir())
        self.script_test_dir = Path(__file__).resolve().parent
        self.script_dir = self.script_test_dir.parent
        self.cur_dir = os.getcwd()
        self.testdata_dir = os.path.join(self.cur_dir, 'testdata')
        self.test_base_dir = os.path.join(self.cur_dir, 'test_results')
        self.adb = AdbHelper(enable_switch_to_root=True)
        self.android_version = self.adb.get_android_version()
        self.device_features = None
        self.browser_option = []
        self.progress_fh = None
        self.ndk_path = None

    def testdata_path(self, testdata_name):
        """ Return the path of a test data. """
        return os.path.join(self.testdata_dir, testdata_name.replace('/', os.sep))

    def test_dir(self, test_name):
        """ Return the dir to run a test. """
        return os.path.join(self.test_base_dir, test_name)

    def script_path(self, script_name):
        """ Return the dir of python scripts. """
        return os.path.join(self.script_dir, script_name)

    def get_device_features(self):
        if self.device_features is None:
            args = [sys.executable, self.script_path(
                'run_simpleperf_on_device.py'), 'list', '--show-features']
            output = subprocess.check_output(args, stderr=TEST_LOGGER.log_fh)
            output = bytes_to_str(output)
            self.device_features = output.split()
        return self.device_features

    def is_trace_offcpu_supported(self):
        return 'trace-offcpu' in self.get_device_features()

    def build_testdata(self):
        """ Collect testdata in self.testdata_dir.
            In system/extras/simpleperf/scripts, testdata comes from:
                <script_dir>/../testdata, <script_dir>/test/script_testdata, <script_dir>/../demo
            In prebuilts/simpleperf, testdata comes from:
                <script_dir>/testdata
        """
        if os.path.isdir(self.testdata_dir):
            return  # already built
        os.makedirs(self.testdata_dir)

        source_dirs = [
            self.script_test_dir / 'script_testdata',
            self.script_dir.parent / 'testdata',
            self.script_dir.parent / 'demo',
            self.script_dir / 'testdata',
        ]

        for source_dir in source_dirs:
            if not source_dir.is_dir():
                continue
            for src_path in source_dir.iterdir():
                dest_path = Path(self.testdata_dir) / src_path.name
                if dest_path.exists():
                    continue
                if src_path.is_file():
                    shutil.copyfile(src_path, dest_path)
                elif src_path.is_dir():
                    shutil.copytree(src_path, dest_path)

    def get_32bit_abi(self):
        return self.adb.get_property('ro.product.cpu.abilist32').strip().split(',')[0]

    def write_progress(self, progress):
        if self.progress_fh:
            self.progress_fh.write(progress + '\n')
            self.progress_fh.flush()


TEST_HELPER = TestHelper()


class TestBase(unittest.TestCase):
    def setUp(self):
        """ Run each test in a separate dir. """
        self.test_dir = TEST_HELPER.test_dir('%s.%s' % (
            self.__class__.__name__, self._testMethodName))
        os.makedirs(self.test_dir)
        self.saved_cwd = os.getcwd()
        os.chdir(self.test_dir)
        TEST_LOGGER.writeln('begin test %s.%s' % (self.__class__.__name__, self._testMethodName))
        self.start_time = time.time()

    def run(self, result=None):
        ret = super(TestBase, self).run(result)
        if result.errors and result.errors[-1][0] == self:
            status = 'FAILED'
            err_info = result.errors[-1][1]
        elif result.failures and result.failures[-1][0] == self:
            status = 'FAILED'
            err_info = result.failures[-1][1]
        else:
            status = 'OK'

        time_taken = time.time() - self.start_time
        TEST_LOGGER.writeln(
            'end test %s.%s %s (%.3fs)' %
            (self.__class__.__name__, self._testMethodName, status, time_taken))
        if status != 'OK':
            TEST_LOGGER.writeln(err_info)

        # Remove test data for passed tests to save space.
        os.chdir(self.saved_cwd)
        if status == 'OK':
            shutil.rmtree(self.test_dir)
        TEST_HELPER.write_progress(
            '%s.%s  %s' % (self.__class__.__name__, self._testMethodName, status))
        return ret

    def run_cmd(self, args, return_output=False, drop_output=True):
        if args[0] == 'report_html.py' or args[0] == INFERNO_SCRIPT:
            args += TEST_HELPER.browser_option
        if TEST_HELPER.ndk_path:
            if args[0] in ['app_profiler.py', 'binary_cache_builder.py', 'pprof_proto_generator.py',
                           'report_html.py']:
                args += ['--ndk_path', TEST_HELPER.ndk_path]
        if args[0].endswith('.py'):
            args = [sys.executable, TEST_HELPER.script_path(args[0])] + args[1:]
        use_shell = args[0].endswith('.bat')
        try:
            if return_output:
                stdout_fd = subprocess.PIPE
                drop_output = False
            elif drop_output:
                stdout_fd = subprocess.DEVNULL
            else:
                stdout_fd = None

            subproc = subprocess.Popen(args, stdout=stdout_fd,
                                       stderr=TEST_LOGGER.log_fh, shell=use_shell)
            stdout_data, _ = subproc.communicate()
            output_data = bytes_to_str(stdout_data)
            returncode = subproc.returncode

        except OSError:
            returncode = None
        self.assertEqual(returncode, 0, msg="failed to run cmd: %s" % args)
        if return_output:
            return output_data
        return ''

    def check_strings_in_file(self, filename, strings):
        self.check_exist(filename=filename)
        with open(filename, 'r') as fh:
            self.check_strings_in_content(fh.read(), strings)

    def check_exist(self, filename=None, dirname=None):
        if filename:
            self.assertTrue(os.path.isfile(filename), filename)
        if dirname:
            self.assertTrue(os.path.isdir(dirname), dirname)

    def check_strings_in_content(self, content, strings):
        fulfilled = [content.find(s) != -1 for s in strings]
        self.check_fulfilled_entries(fulfilled, strings)

    def check_fulfilled_entries(self, fulfilled, entries):
        failed_entries = []
        for ok, entry in zip(fulfilled, entries):
            if not ok:
                failed_entries.append(entry)

        if failed_entries:
            self.fail('failed in below entries: %s' % (failed_entries,))
