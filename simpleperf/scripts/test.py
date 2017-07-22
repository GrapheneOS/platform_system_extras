#!/usr/bin/env python
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

Test using both `adb root` and `adb unroot`.

"""

import os
import re
import shutil
import sys
import tempfile
import unittest
from utils import *

has_google_protobuf = True
try:
    import google.protobuf
except:
    has_google_protobuf = False


class TestExamplesBase(unittest.TestCase):
    @classmethod
    def prepare(cls, example_name, package_name, activity_name, abi=None, adb_root=False):
        cls.adb = AdbHelper(enable_switch_to_root=adb_root)
        cls.example_path = os.path.join("testdata", example_name)
        if not os.path.isdir(cls.example_path):
            log_fatal("can't find " + cls.example_path)
        cls.apk_path = os.path.join(cls.example_path, "app-profiling.apk")
        if not os.path.isfile(cls.apk_path):
            log_fatal("can't find " + cls.apk_path)
        cls.package_name = package_name
        cls.activity_name = activity_name
        cls.python_path = sys.executable
        args = ["install", "-r"]
        if abi:
            args += ["--abi", abi]
        args.append(cls.apk_path)
        cls.adb.check_run(args)
        cls.adb_root = adb_root
        cls.compiled = False

    @classmethod
    def tearDownClass(cls):
        cls.adb.check_run(["uninstall", cls.package_name])

    @classmethod
    def cleanupTestFiles(cls):
        cls.remove("binary_cache")
        cls.remove("annotated_files")
        cls.remove("perf.data")
        cls.remove("report.txt")
        cls.remove("pprof.profile")

    def run_cmd(self, args, return_output=False):
        args = [self.python_path] + args
        try:
            if not return_output:
                returncode = subprocess.call(args)
            else:
                subproc = subprocess.Popen(args, stdout=subprocess.PIPE)
                (output_data, _) = subproc.communicate()
                returncode = subproc.returncode
        except:
            returncode = None
        self.assertEqual(returncode, 0, msg="failed to run cmd: %s" % args)
        if return_output:
            return output_data

    def run_app_profiler(self, record_arg = "-g --duration 3 -e cpu-cycles:u",
                         build_binary_cache=True, skip_compile=False, start_activity=True,
                         native_lib_dir=None):
        args = ["app_profiler.py", "--app", self.package_name, "--apk", self.apk_path,
                "-a", self.activity_name, "-r", record_arg, "-o", "perf.data"]
        if not build_binary_cache:
            args.append("-nb")
        if skip_compile or self.__class__.compiled:
            args.append("-nc")
        if start_activity:
            args += ["-a", self.activity_name]
        if native_lib_dir:
            args += ["-lib", native_lib_dir]
        if not self.adb_root:
            args.append("--disable_adb_root")
        self.run_cmd(args)
        self.check_exist(file="perf.data")
        if build_binary_cache:
            self.check_exist(dir="binary_cache")
        if not skip_compile:
            self.__class__.compiled = True

    @classmethod
    def remove(cls, dir):
        shutil.rmtree(dir, ignore_errors=True)

    def check_exist(self, file=None, dir=None):
        if file:
            self.assertTrue(os.path.isfile(file), file)
        if dir:
            self.assertTrue(os.path.isdir(dir), dir)

    def check_file_under_dir(self, dir, file):
        self.check_exist(dir=dir)
        for _, _, files in os.walk(dir):
            for f in files:
                if f == file:
                    return
        self.fail("Failed to call check_file_under_dir(dir=%s, file=%s)" % (dir, file))


    def check_strings_in_file(self, file, strings):
        self.check_exist(file=file)
        with open(file, 'r') as fh:
            self.check_strings_in_content(fh.read(), strings)

    def check_strings_in_content(self, content, strings):
        for s in strings:
            self.assertNotEqual(content.find(s), -1, "s: %s, content: %s" % (s, content))

    def check_annotation_summary(self, summary_file, check_entries):
        """ check_entries is a list of (name, accumulated_period, period).
            This function checks for each entry, if the line containing [name]
            has at least required accumulated_period and period.
        """
        self.check_exist(file=summary_file)
        with open(summary_file, 'r') as fh:
            summary = fh.read()
        fulfilled = [False for x in check_entries]
        if not hasattr(self, "summary_check_re"):
            self.summary_check_re = re.compile(r'accumulated_period:\s*([\d.]+)%.*period:\s*([\d.]+)%')
        for line in summary.split('\n'):
            for i in range(len(check_entries)):
                (name, need_acc_period, need_period) = check_entries[i]
                if not fulfilled[i] and line.find(name) != -1:
                    m = self.summary_check_re.search(line)
                    if m:
                        acc_period = float(m.group(1))
                        period = float(m.group(2))
                        if acc_period >= need_acc_period and period >= need_period:
                            fulfilled[i] = True
        self.assertEqual(len(fulfilled), sum([int(x) for x in fulfilled]), fulfilled)

    def common_test_app_profiler(self):
        self.run_cmd(["app_profiler.py", "-h"])
        self.remove("binary_cache")
        self.run_app_profiler(build_binary_cache=False)
        self.assertFalse(os.path.isdir("binary_cache"))
        args = ["binary_cache_builder.py"]
        if not self.adb_root:
            args.append("--disable_adb_root")
        self.run_cmd(args)
        self.check_exist(dir="binary_cache")
        self.remove("binary_cache")
        self.run_app_profiler(build_binary_cache=True)
        self.run_app_profiler(skip_compile=True)
        self.run_app_profiler(start_activity=False)


    def common_test_report(self):
        self.run_cmd(["report.py", "-h"])
        self.run_app_profiler(build_binary_cache=False)
        self.run_cmd(["report.py"])
        self.run_cmd(["report.py", "-i", "perf.data"])
        self.run_cmd(["report.py", "-g"])
        self.run_cmd(["report.py", "--self-kill-for-testing",  "-g", "--gui"])

    def common_test_annotate(self):
        self.run_cmd(["annotate.py", "-h"])
        self.run_app_profiler()
        self.remove("annotated_files")
        self.run_cmd(["annotate.py", "-s", self.example_path])
        self.check_exist(dir="annotated_files")

    def common_test_report_sample(self, check_strings):
        self.run_cmd(["report_sample.py", "-h"])
        self.remove("binary_cache")
        self.run_app_profiler(build_binary_cache=False)
        self.run_cmd(["report_sample.py"])
        output = self.run_cmd(["report_sample.py", "perf.data"], return_output=True)
        self.check_strings_in_content(output, check_strings)
        self.run_app_profiler(record_arg="-g --duration 3 -e cpu-cycles:u, --no-dump-symbols")
        output = self.run_cmd(["report_sample.py", "--symfs", "binary_cache"], return_output=True)
        self.check_strings_in_content(output, check_strings)

    def common_test_pprof_proto_generator(self, check_strings_with_lines,
                                          check_strings_without_lines):
        if not has_google_protobuf:
            log_info('Skip test for pprof_proto_generator because google.protobuf is missing')
            return
        self.run_cmd(["pprof_proto_generator.py", "-h"])
        self.run_app_profiler()
        self.run_cmd(["pprof_proto_generator.py"])
        self.remove("pprof.profile")
        self.run_cmd(["pprof_proto_generator.py", "-i", "perf.data", "-o", "pprof.profile"])
        self.check_exist(file="pprof.profile")
        self.run_cmd(["pprof_proto_generator.py", "--show"])
        output = self.run_cmd(["pprof_proto_generator.py", "--show", "pprof.profile"],
                              return_output=True)
        self.check_strings_in_content(output, check_strings_with_lines +
                                              ["has_line_numbers: True"])
        self.remove("binary_cache")
        self.run_cmd(["pprof_proto_generator.py"])
        output = self.run_cmd(["pprof_proto_generator.py", "--show", "pprof.profile"],
                              return_output=True)
        self.check_strings_in_content(output, check_strings_without_lines +
                                              ["has_line_numbers: False"])


class TestExamplePureJava(TestExamplesBase):
    @classmethod
    def setUpClass(cls):
        cls.prepare("SimpleperfExamplePureJava",
                    "com.example.simpleperf.simpleperfexamplepurejava",
                    ".MainActivity")

    def test_app_profiler(self):
        self.common_test_app_profiler()

    def test_report(self):
        self.common_test_report()
        self.run_cmd(["report.py", "-g", "-o", "report.txt"])
        self.check_strings_in_file("report.txt",
            ["com.example.simpleperf.simpleperfexamplepurejava.MainActivity$1.run()",
             "__start_thread"])

    def test_annotate(self):
        self.common_test_annotate()
        self.check_file_under_dir("annotated_files", "MainActivity.java")
        summary_file = os.path.join("annotated_files", "summary")
        self.check_annotation_summary(summary_file,
            [("MainActivity.java", 80, 80),
             ("run", 80, 0),
             ("callFunction", 0, 0),
             ("line 24", 80, 0)])

    def test_report_sample(self):
        self.common_test_report_sample(
            ["com.example.simpleperf.simpleperfexamplepurejava.MainActivity$1.run()",
             "__start_thread"])

    def test_pprof_proto_generator(self):
        self.common_test_pprof_proto_generator(
            check_strings_with_lines=
                ["com/example/simpleperf/simpleperfexamplepurejava/MainActivity.java",
                 "run"],
            check_strings_without_lines=
                ["com.example.simpleperf.simpleperfexamplepurejava.MainActivity$1.run()"])


class TestExamplePureJavaRoot(TestExamplesBase):
    @classmethod
    def setUpClass(cls):
        cls.prepare("SimpleperfExamplePureJava",
                    "com.example.simpleperf.simpleperfexamplepurejava",
                    ".MainActivity",
                    adb_root=True)

    def test_app_profiler(self):
        self.common_test_app_profiler()


class TestExampleWithNative(TestExamplesBase):
    @classmethod
    def setUpClass(cls):
        cls.prepare("SimpleperfExampleWithNative",
                    "com.example.simpleperf.simpleperfexamplewithnative",
                    ".MainActivity")

    def test_app_profiler(self):
        self.common_test_app_profiler()
        self.remove("binary_cache")
        self.run_app_profiler(native_lib_dir=self.example_path)

    def test_report(self):
        self.common_test_report()
        self.run_cmd(["report.py", "-g", "-o", "report.txt"])
        self.check_strings_in_file("report.txt",
            ["BusyLoopThread",
             "__start_thread"])

    def test_annotate(self):
        self.common_test_annotate()
        self.check_file_under_dir("annotated_files", "native-lib.cpp")
        summary_file = os.path.join("annotated_files", "summary")
        self.check_annotation_summary(summary_file,
            [("native-lib.cpp", 20, 0),
             ("BusyLoopThread", 20, 0),
             ("line 46", 20, 0)])

    def test_report_sample(self):
        self.common_test_report_sample(
            ["BusyLoopThread",
             "__start_thread"])

    def test_pprof_proto_generator(self):
        self.common_test_pprof_proto_generator(
            check_strings_with_lines=
                ["native-lib.cpp",
                 "BusyLoopThread"],
            check_strings_without_lines=
                ["BusyLoopThread"])


class TestExampleWithNativeRoot(TestExamplesBase):
    @classmethod
    def setUpClass(cls):
        cls.prepare("SimpleperfExampleWithNative",
                    "com.example.simpleperf.simpleperfexamplewithnative",
                    ".MainActivity",
                    adb_root=True)

    def test_app_profiler(self):
        self.common_test_app_profiler()
        self.remove("binary_cache")
        self.run_app_profiler(native_lib_dir=self.example_path)


class TestExampleWithNativeForceArm(TestExampleWithNative):
    @classmethod
    def setUpClass(cls):
        cls.prepare("SimpleperfExampleWithNative",
                    "com.example.simpleperf.simpleperfexamplewithnative",
                    ".MainActivity",
                    abi="armeabi-v7a")


class TestExampleWithNativeForceArmRoot(TestExampleWithNativeRoot):
    @classmethod
    def setUpClass(cls):
        cls.prepare("SimpleperfExampleWithNative",
                    "com.example.simpleperf.simpleperfexamplewithnative",
                    ".MainActivity",
                    abi="armeabi-v7a",
                    adb_root=False)


class TestExampleOfKotlin(TestExamplesBase):
    @classmethod
    def setUpClass(cls):
        cls.prepare("SimpleperfExampleOfKotlin",
                    "com.example.simpleperf.simpleperfexampleofkotlin",
                    ".MainActivity")

    def test_app_profiler(self):
        self.common_test_app_profiler()

    def test_report(self):
        self.common_test_report()
        self.run_cmd(["report.py", "-g", "-o", "report.txt"])
        self.check_strings_in_file("report.txt",
            ["com.example.simpleperf.simpleperfexampleofkotlin.MainActivity$createBusyThread$1.run()",
             "__start_thread"])

    def test_annotate(self):
        self.common_test_annotate()
        self.check_file_under_dir("annotated_files", "MainActivity.kt")
        summary_file = os.path.join("annotated_files", "summary")
        self.check_annotation_summary(summary_file,
            [("MainActivity.kt", 80, 80),
             ("run", 80, 0),
             ("callFunction", 0, 0),
             ("line 19", 80, 0),
             ("line 25", 0, 0)])

    def test_report_sample(self):
        self.common_test_report_sample(
            ["com.example.simpleperf.simpleperfexampleofkotlin.MainActivity$createBusyThread$1.run()",
             "__start_thread"])

    def test_pprof_proto_generator(self):
        self.common_test_pprof_proto_generator(
            check_strings_with_lines=
                ["com/example/simpleperf/simpleperfexampleofkotlin/MainActivity.kt",
                 "run"],
            check_strings_without_lines=
                ["com.example.simpleperf.simpleperfexampleofkotlin.MainActivity$createBusyThread$1.run()"])


class TestExampleOfKotlinRoot(TestExamplesBase):
    @classmethod
    def setUpClass(cls):
        cls.prepare("SimpleperfExampleOfKotlin",
                    "com.example.simpleperf.simpleperfexampleofkotlin",
                    ".MainActivity",
                    adb_root=True)

    def test_app_profiler(self):
        self.common_test_app_profiler()


if __name__ == '__main__':
    test_program = unittest.main(failfast=True, exit=False)
    if test_program.result.wasSuccessful():
        TestExamplesBase.cleanupTestFiles()