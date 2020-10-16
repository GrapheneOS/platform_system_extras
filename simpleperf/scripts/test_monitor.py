#!/usr/bin/env python3
#
# Copyright (C) 2020 The Android Open Source Project
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
"""test_monitor.py: Run test.py and report test summary.

1. Support testing multiple devices at the same time.
2. Support repeating tests.
3. Continue test if test.py aborts in the middle.
4. Handle both parallel tests and serialized tests.

"""

import argparse
import collections
import fnmatch
import os
import re
import shutil
import subprocess
import sys
import time

from simpleperf_utils import AdbHelper, log_exit, log_info

Device = collections.namedtuple('Device', ['name', 'serial_number'])

Config = collections.namedtuple(
    'Config', ['devices', 'python_interpreters', 'repeat_times', 'test_patterns', 'test_dir'])

CONFIG = None

TEST_TIMEOUT_IN_SEC = 180


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '-d', '--device', nargs='+',
        help='set devices used to run tests. Each device in format name:serial-number')
    parser.add_argument('--python-version', choices=['2', '3', 'both'], default='both', help="""
                        Run tests on which python versions.""")
    parser.add_argument('-r', '--repeat', type=int, default=1, help='times to repeat tests')
    parser.add_argument('--test-dir', default='.', help='test dir')
    parser.add_argument('-p', '--pattern', nargs='+',
                        help='Run tests matching the selected pattern.')
    args = parser.parse_args()

    devices = []
    if args.device:
        for s in args.device:
            name, serial_number = s.split(':')
            devices.append(Device(name=name, serial_number=serial_number))
    else:
        devices.append(Device(name='default', serial_number=None))

    if args.python_version == '2':
        python_interpreters = ['python']
    elif args.python_version == '3':
        python_interpreters = ['python3']
    else:
        python_interpreters = ['python', 'python3']

    global CONFIG
    CONFIG = Config(devices=devices, python_interpreters=python_interpreters,
                    repeat_times=args.repeat, test_patterns=args.pattern, test_dir=args.test_dir)
    log_info('config = %s' % (CONFIG,))

def get_test_script_path():
    return os.path.join(os.path.dirname(os.path.realpath(__file__)), 'test.py')


def is_serialized_test(test):
    return test in ['TestExamplePureJava.test_run_simpleperf_without_usb_connection']


def get_tests():
    res = subprocess.run('%s %s --list-tests' % (sys.executable, get_test_script_path()),
                         check=True, shell=True, stdout=subprocess.PIPE, encoding='utf-8')
    tests = [x.strip() for x in res.stdout.split('\n') if x.strip()]
    if CONFIG.test_patterns:
        patterns = [re.compile(fnmatch.translate(x)) for x in CONFIG.test_patterns]
        tests = [t for t in tests if any(pattern.match(t) for pattern in patterns)]
    parallel_tests = [t for t in tests if not is_serialized_test(t)]
    serialized_tests = [t for t in tests if is_serialized_test(t)]
    if not parallel_tests and not serialized_tests:
        log_exit('no test to run')
    return parallel_tests, serialized_tests


class Task:
    """ Run test.py one time for a device and python interpreter.
    """

    def __init__(self, name, device, python_interpreter, tests, repeat_index):
        self.name = name
        self.device = device
        self.python_interpreter = python_interpreter
        self.tests = tests
        self.repeat_index = repeat_index
        self.try_time = 0
        self.test_results = {}
        self.test_proc = None
        self.test_dir = None
        self.progress_file = None
        self.failed_to_setup_test = None
        self.last_update_time = None

    def start_task(self):
        assert not self.test_proc
        not_started_tests = [t for t in self.tests if t not in self.test_results]
        assert not_started_tests
        self.try_time += 1
        self._create_test_dir()
        with open(os.path.join(self.test_dir, 'tests.txt'), 'w') as fh:
            fh.write('\n'.join(not_started_tests) + '\n')

        self.progress_file = os.path.join(self.test_dir, 'progress_file.txt')
        env = os.environ.copy()
        if self.device.serial_number:
            env['ANDROID_SERIAL'] = self.device.serial_number
        if not self._is_device_available():
            for t in self.tests:
                if t not in self.test_results:
                    self.test_results[t] = 'DEVICE_NOT_AVAILABLE'
            return
        args = [self.python_interpreter, get_test_script_path(),
                '--progress-file', self.progress_file]
        args += not_started_tests
        self.test_proc = subprocess.Popen(args, cwd=self.test_dir, env=env)
        self.last_update_time = time.time()
        log_info('start task for %s' % self.test_dir)

    def _create_test_dir(self):
        test_name = '%s_%s_%s_repeat%d_try%d' % (
            self.name, self.device.name, self.python_interpreter, self.repeat_index, self.try_time)
        self.test_dir = os.path.abspath(test_name)
        if os.path.isdir(self.test_dir):
            shutil.rmtree(self.test_dir)
        os.mkdir(self.test_dir)

    def _is_device_available(self):
        adb = AdbHelper()
        if self.device.serial_number:
            adb.serial_number = self.device.serial_number
        res, _ = adb.run_and_return_output(['shell', 'pwd'], log_output=False, log_stderr=False)
        return res

    def update_status(self):
        if self.finished():
            return
        assert self.test_proc
        has_update = False
        if os.path.isfile(self.progress_file):
            with open(self.progress_file, 'r', encoding='utf-8') as fh:
                for line in fh.readlines():
                    items = line.strip().split()
                    if len(items) == 2 and items[1] in ['OK', 'FAILED']:
                        if self.test_results.get(items[0]) != items[1]:
                            has_update = True
                            self.test_results[items[0]] = items[1]
        if has_update:
            self.last_update_time = time.time()
        elif time.time() - self.last_update_time > TEST_TIMEOUT_IN_SEC:
            self.test_proc.kill()

        if self.test_proc.poll() is not None:
            self.test_proc = None
            if not self.finished():
                # Test process ends without finishing all tests.
                # It should be caused by failing to setup a test.
                for t in self.tests:
                    if t not in self.test_results:
                        self._on_failed_to_setup_test(t)
                        break

    def _on_failed_to_setup_test(self, test):
        if self.failed_to_setup_test is None or self.failed_to_setup_test[0] != test:
            self.failed_to_setup_test = [test, 1]
        else:
            self.failed_to_setup_test[1] += 1
            # Mark a test as failed if failing to setup it for multiple times.
            if self.failed_to_setup_test[1] == 5:
                self.test_results[test] = 'FAILED_TO_SETUP'

    def started(self):
        return self.test_proc is not None

    def finished(self):
        return all([t in self.test_results for t in self.tests])

    def enumerate_new_task(self):
        """ When one task finishes, it can enumerate a new task to run, for repeat testing or
            another python version.
        """
        if self.repeat_index < CONFIG.repeat_times:
            return Task(
                self.name, self.device, self.python_interpreter, self.tests, self.repeat_index + 1)
        if self.python_interpreter != CONFIG.python_interpreters[-1]:
            i = CONFIG.python_interpreters.index(self.python_interpreter)
            return Task(self.name, self.device, CONFIG.python_interpreters[i+1], self.tests, 1)
        return None

    def force_stop(self):
        if self.test_proc:
            self.test_proc.kill()


class TestSummary:
    def __init__(self, tests):
        tests.sort()
        self.results = {}
        for device in CONFIG.devices:
            for python_interpreter in CONFIG.python_interpreters:
                results = self.results['%s_%s' % (device.name, python_interpreter)] = {}
                for test in tests:
                    for repeat_index in range(1, CONFIG.repeat_times + 1):
                        test_name = '%s_repeat_%d' % (test, repeat_index)
                        results[test_name] = 'None'

    def update(self, task):
        config_name = '%s_%s' % (task.device.name, task.python_interpreter)
        results = self.results[config_name]
        for t, status in task.test_results.items():
            test_name = '%s_repeat_%d' % (t, task.repeat_index)
            results[test_name] = status

        self._write_summary()

    def _write_summary(self):
        with open('test_summary.txt', 'w') as fh:
            for config_name in sorted(self.results):
                fh.write('%s\n' % config_name)
                results = self.results[config_name]
                for test_name in sorted(results):
                    result = results[test_name]
                    fh.write('\t%s: %s\n' % (test_name, result))

        with open('failed_test_summary.txt', 'w') as fh:
            for config_name in sorted(self.results):
                results = self.results[config_name]
                has_failure = any(result not in ['OK', 'None'] for result in results.values())
                if not has_failure:
                    continue
                fh.write('%s\n' % config_name)
                for test_name in sorted(results):
                    result = results[test_name]
                    if result not in ['OK', 'None']:
                        fh.write('\t%s: %s\n' % (test_name, result))


def run_tasks(tasks, test_summary):
    try:
        while tasks:
            for task in tasks:
                if not task.started():
                    task.start_task()
                task.update_status()
                test_summary.update(task)
            new_tasks = []
            for task in tasks:
                if task.finished():
                    new_task = task.enumerate_new_task()
                    if new_task:
                        new_tasks.append(new_task)
                else:
                    new_tasks.append(task)
            tasks = new_tasks
            time.sleep(2)
    except KeyboardInterrupt:
        for task in tasks:
            task.force_stop()


def main():
    parse_args()
    parallel_tests, serialized_tests = get_tests()
    test_summary = TestSummary(parallel_tests + serialized_tests)
    if parallel_tests:
        tasks = []
        for device in CONFIG.devices:
            tasks.append(
                Task(
                    'test', device, CONFIG.python_interpreters[0],
                    parallel_tests, 1))
        run_tasks(tasks, test_summary)
    if serialized_tests:
        for device in CONFIG.devices:
            tasks = [
                Task(
                    'serialized_test', device, CONFIG.python_interpreters[0],
                    serialized_tests, 1)]
            run_tasks(tasks, test_summary)


if __name__ == '__main__':
    main()
