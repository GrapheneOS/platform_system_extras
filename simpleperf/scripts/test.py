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
import signal
import sys
import tempfile
import time
import unittest
from utils import *
from simpleperf_report_lib import ReportLib

has_google_protobuf = True
try:
    import google.protobuf
except:
    has_google_protobuf = False


adb = AdbHelper()
# TODO: Change to check ro.build.version.sdk >= 26 when OMR1 is prevalent.
device_version = adb.check_run_and_return_output(['shell', 'getprop', 'ro.build.version.release'])
support_trace_offcpu = device_version.find('OMR1') != -1

def build_testdata():
    """ Collect testdata from ../testdata and ../demo. """
    from_testdata_path = os.path.join('..', 'testdata')
    from_demo_path = os.path.join('..', 'demo')
    if not os.path.isdir(from_testdata_path) or not os.path.isdir(from_demo_path):
        return
    copy_testdata_list = ['perf_with_symbols.data', 'perf_with_trace_offcpu.data']
    copy_demo_list = ['SimpleperfExamplePureJava', 'SimpleperfExampleWithNative',
                      'SimpleperfExampleOfKotlin']

    testdata_path = "testdata"
    if os.path.isdir(testdata_path):
        shutil.rmtree(testdata_path)
        os.mkdir(testdata_path)
    for testdata in copy_testdata_list:
        shutil.copy(os.path.join(from_testdata_path, testdata), testdata_path)
    for demo in copy_demo_list:
        shutil.copytree(os.path.join(from_demo_path, demo), os.path.join(testdata_path, demo))


class TestExampleBase(unittest.TestCase):
    @classmethod
    def prepare(cls, example_name, package_name, activity_name, abi=None, adb_root=False):
        cls.adb = AdbHelper(enable_switch_to_root=adb_root)
        cls.example_path = os.path.join("testdata", example_name)
        if not os.path.isdir(cls.example_path):
            log_fatal("can't find " + cls.example_path)
        for root, _, files in os.walk(cls.example_path):
            if 'app-profiling.apk' in files:
                cls.apk_path = os.path.join(root, 'app-profiling.apk')
                break
        if not hasattr(cls, 'apk_path'):
            log_fatal("can't find app-profiling.apk under " + cls.example_path)
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

    def setUp(self):
        if self.id().find('TraceOffCpu') != -1 and not support_trace_offcpu:
            self.skipTest('trace-offcpu is not supported on device')

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


class TestExamplePureJava(TestExampleBase):
    @classmethod
    def setUpClass(cls):
        cls.prepare("SimpleperfExamplePureJava",
                    "com.example.simpleperf.simpleperfexamplepurejava",
                    ".MainActivity")

    def test_app_profiler(self):
        self.common_test_app_profiler()

    def test_app_profiler_with_ctrl_c(self):
        if is_windows():
            return
        args = [sys.executable, "app_profiler.py", "--app", self.package_name,
                "-r", "--duration 10000", "-nc"]
        subproc = subprocess.Popen(args)
        time.sleep(3)

        subproc.send_signal(signal.SIGINT)
        subproc.wait()
        self.assertEqual(subproc.returncode, 0)
        self.run_cmd(["report.py"])

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


class TestExamplePureJavaRoot(TestExampleBase):
    @classmethod
    def setUpClass(cls):
        cls.prepare("SimpleperfExamplePureJava",
                    "com.example.simpleperf.simpleperfexamplepurejava",
                    ".MainActivity",
                    adb_root=True)

    def test_app_profiler(self):
        self.common_test_app_profiler()


class TestExamplePureJavaTraceOffCpu(TestExampleBase):
    @classmethod
    def setUpClass(cls):
        cls.prepare("SimpleperfExamplePureJava",
                    "com.example.simpleperf.simpleperfexamplepurejava",
                    ".SleepActivity")

    def test_smoke(self):
        self.run_app_profiler(record_arg = "-g --duration 3 -e cpu-cycles:u --trace-offcpu")
        self.run_cmd(["report.py", "-g", "-o", "report.txt"])
        self.check_strings_in_file("report.txt",
            ["com.example.simpleperf.simpleperfexamplepurejava.SleepActivity$1.run()",
             "long com.example.simpleperf.simpleperfexamplepurejava.SleepActivity$1.RunFunction()",
             "long com.example.simpleperf.simpleperfexamplepurejava.SleepActivity$1.SleepFunction(long)"
             ])
        self.remove("annotated_files")
        self.run_cmd(["annotate.py", "-s", self.example_path])
        self.check_exist(dir="annotated_files")
        self.check_file_under_dir("annotated_files", "SleepActivity.java")
        summary_file = os.path.join("annotated_files", "summary")
        self.check_annotation_summary(summary_file,
            [("SleepActivity.java", 80, 20),
             ("run", 80, 0),
             ("RunFunction", 20, 20),
             ("SleepFunction", 20, 0),
             ("line 24", 20, 0),
             ("line 32", 20, 0)])


class TestExampleWithNative(TestExampleBase):
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


class TestExampleWithNativeRoot(TestExampleBase):
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


class TestExampleWithNativeTraceOffCpu(TestExampleBase):
    @classmethod
    def setUpClass(cls):
        cls.prepare("SimpleperfExampleWithNative",
                    "com.example.simpleperf.simpleperfexamplewithnative",
                    ".SleepActivity")

    def test_smoke(self):
        self.run_app_profiler(record_arg = "-g --duration 3 -e cpu-cycles:u --trace-offcpu")
        self.run_cmd(["report.py", "-g", "--comms", "SleepThread", "-o", "report.txt"])
        self.check_strings_in_file("report.txt",
            ["SleepThread(void*)",
             "RunFunction()",
             "SleepFunction(unsigned long long)"])
        self.remove("annotated_files")
        self.run_cmd(["annotate.py", "-s", self.example_path, "--comm", "SleepThread"])
        self.check_exist(dir="annotated_files")
        self.check_file_under_dir("annotated_files", "native-lib.cpp")
        summary_file = os.path.join("annotated_files", "summary")
        self.check_annotation_summary(summary_file,
            [("native-lib.cpp", 80, 20),
             ("SleepThread", 80, 0),
             ("RunFunction", 20, 20),
             ("SleepFunction", 20, 0),
             ("line 73", 20, 0),
             ("line 83", 20, 0)])


class TestExampleWithNativeJniCall(TestExampleBase):
    @classmethod
    def setUpClass(cls):
        cls.prepare("SimpleperfExampleWithNative",
                    "com.example.simpleperf.simpleperfexamplewithnative",
                    ".MixActivity")

    def test_smoke(self):
        self.run_app_profiler()
        self.run_cmd(["report.py", "-g", "--comms", "BusyThread", "-o", "report.txt"])
        self.check_strings_in_file("report.txt",
            ["void com.example.simpleperf.simpleperfexamplewithnative.MixActivity$1.run()",
             "int com.example.simpleperf.simpleperfexamplewithnative.MixActivity.callFunction(int)",
             "Java_com_example_simpleperf_simpleperfexamplewithnative_MixActivity_callFunction"])
        self.remove("annotated_files")
        self.run_cmd(["annotate.py", "-s", self.example_path, "--comm", "BusyThread"])
        self.check_exist(dir="annotated_files")
        self.check_file_under_dir("annotated_files", "native-lib.cpp")
        self.check_file_under_dir("annotated_files", "MixActivity.java")
        summary_file = os.path.join("annotated_files", "summary")
        self.check_annotation_summary(summary_file,
            [("MixActivity.java", 80, 0),
             ("run", 80, 0),
             ("line 26", 20, 0),
             ("native-lib.cpp", 10, 0),
             ("line 40", 10, 0)])


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


class TestExampleWithNativeTraceOffCpuForceArm(TestExampleWithNativeTraceOffCpu):
    @classmethod
    def setUpClass(cls):
        cls.prepare("SimpleperfExampleWithNative",
                    "com.example.simpleperf.simpleperfexamplewithnative",
                    ".SleepActivity",
                    abi="armeabi-v7a")


class TestExampleOfKotlin(TestExampleBase):
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


class TestExampleOfKotlinRoot(TestExampleBase):
    @classmethod
    def setUpClass(cls):
        cls.prepare("SimpleperfExampleOfKotlin",
                    "com.example.simpleperf.simpleperfexampleofkotlin",
                    ".MainActivity",
                    adb_root=True)

    def test_app_profiler(self):
        self.common_test_app_profiler()


class TestExampleOfKotlinTraceOffCpu(TestExampleBase):
    @classmethod
    def setUpClass(cls):
        cls.prepare("SimpleperfExampleOfKotlin",
                    "com.example.simpleperf.simpleperfexampleofkotlin",
                    ".SleepActivity")

    def test_smoke(self):
        self.run_app_profiler(record_arg = "-g --duration 3 -e cpu-cycles:u --trace-offcpu")
        self.run_cmd(["report.py", "-g", "-o", "report.txt"])
        self.check_strings_in_file("report.txt",
            ["void com.example.simpleperf.simpleperfexampleofkotlin.SleepActivity$createRunSleepThread$1.run()",
             "long com.example.simpleperf.simpleperfexampleofkotlin.SleepActivity$createRunSleepThread$1.RunFunction()",
             "long com.example.simpleperf.simpleperfexampleofkotlin.SleepActivity$createRunSleepThread$1.SleepFunction(long)"
             ])
        self.remove("annotated_files")
        self.run_cmd(["annotate.py", "-s", self.example_path])
        self.check_exist(dir="annotated_files")
        self.check_file_under_dir("annotated_files", "SleepActivity.kt")
        summary_file = os.path.join("annotated_files", "summary")
        self.check_annotation_summary(summary_file,
            [("SleepActivity.kt", 80, 20),
             ("run", 80, 0),
             ("RunFunction", 20, 20),
             ("SleepFunction", 20, 0),
             ("line 24", 20, 0),
             ("line 32", 20, 0)])


class TestReportLib(unittest.TestCase):
    def setUp(self):
        self.report_lib = ReportLib()
        self.report_lib.SetRecordFile(os.path.join('testdata', 'perf_with_symbols.data'))

    def tearDown(self):
        self.report_lib.Close()

    def test_build_id(self):
        build_id = self.report_lib.GetBuildIdForPath('/data/t2')
        self.assertEqual(build_id, '0x70f1fe24500fc8b0d9eb477199ca1ca21acca4de')

    def test_symbol_addr(self):
        found_func2 = False
        while self.report_lib.GetNextSample():
            sample = self.report_lib.GetCurrentSample()
            symbol = self.report_lib.GetSymbolOfCurrentSample()
            if symbol.symbol_name == 'func2(int, int)':
                found_func2 = True
                self.assertEqual(symbol.symbol_addr, 0x4004ed)
        self.assertTrue(found_func2)

    def test_sample(self):
        found_sample = False
        while self.report_lib.GetNextSample():
            sample = self.report_lib.GetCurrentSample()
            if sample.ip == 0x4004ff and sample.time == 7637889424953:
                found_sample = True
                self.assertEqual(sample.pid, 15926)
                self.assertEqual(sample.tid, 15926)
                self.assertEqual(sample.thread_comm, 't2')
                self.assertEqual(sample.cpu, 5)
                self.assertEqual(sample.period, 694614)
                event = self.report_lib.GetEventOfCurrentSample()
                self.assertEqual(event.name, 'cpu-cycles')
                callchain = self.report_lib.GetCallChainOfCurrentSample()
                self.assertEqual(callchain.nr, 0)
        self.assertTrue(found_sample)

    def test_meta_info(self):
        self.report_lib.SetRecordFile(os.path.join('testdata', 'perf_with_trace_offcpu.data'))
        meta_info = self.report_lib.MetaInfo()
        self.assertEqual(meta_info["simpleperf_version"], "1.65f91c7ed862")
        self.assertEqual(meta_info["system_wide_collection"], "false")
        self.assertEqual(meta_info["trace_offcpu"], "true")
        self.assertEqual(meta_info["event_type_info"], "cpu-cycles,0,0\nsched:sched_switch,2,47")

    def test_event_name_from_meta_info(self):
        self.report_lib.SetRecordFile(os.path.join('testdata', 'perf_with_trace_offcpu.data'))
        event_names = set()
        while self.report_lib.GetNextSample():
            event_names.add(self.report_lib.GetEventOfCurrentSample().name)
        self.assertTrue('sched:sched_switch' in event_names)
        self.assertTrue('cpu-cycles' in event_names)

    def test_offcpu(self):
        self.report_lib.SetRecordFile(os.path.join('testdata', 'perf_with_trace_offcpu.data'))
        total_period = 0
        sleep_function_period = 0
        sleep_function_name = "SleepFunction(unsigned long long)"
        while self.report_lib.GetNextSample():
            sample = self.report_lib.GetCurrentSample()
            total_period += sample.period
            if self.report_lib.GetSymbolOfCurrentSample().symbol_name == sleep_function_name:
                sleep_function_period += sample.period
                continue
            callchain = self.report_lib.GetCallChainOfCurrentSample()
            for i in range(callchain.nr):
                if callchain.entries[i].symbol.symbol_name == sleep_function_name:
                    sleep_function_period += sample.period
                    break
        sleep_percentage = float(sleep_function_period) / total_period
        self.assertAlmostEqual(sleep_percentage, 0.4629, delta=0.0001)


def main():
    build_testdata()
    test_program = unittest.main(failfast=True, exit=False)
    if test_program.result.wasSuccessful():
        TestExampleBase.cleanupTestFiles()

if __name__ == '__main__':
    main()