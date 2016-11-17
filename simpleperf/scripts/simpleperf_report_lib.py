#!/usr/bin/env python
#
# Copyright (C) 2016 The Android Open Source Project
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

"""simpleperf_report_lib.py: a python wrapper of libsimpleperf_report.so.
   Used to access samples in perf.data.
"""

import ctypes as ct
import os
import sys


def _isWindows():
    return sys.platform == 'win32' or sys.platform == 'cygwin'


def _get_script_path():
    return os.path.dirname(os.path.realpath(__file__))


def _get_native_lib():
    if _isWindows():
        so_name = 'libsimpleperf_report.dll'
    elif sys.platform == 'darwin': # OSX
        so_name = 'libsimpleperf_report.dylib'
    else:
        so_name = 'libsimpleperf_report.so'

    return os.path.join(_get_script_path(), so_name)


def _is_null(p):
    return ct.cast(p, ct.c_void_p).value is None


def _char_pt(str):
    if sys.version_info < (3, 0):
        return str
    # In python 3, str are wide strings whereas the C api expects 8 bit strings, hence we have to convert
    # For now using utf-8 as the encoding.
    return str.encode('utf-8')


class SampleStruct(ct.Structure):
    _fields_ = [('ip', ct.c_uint64),
                ('pid', ct.c_uint32),
                ('tid', ct.c_uint32),
                ('thread_comm', ct.c_char_p),
                ('time', ct.c_uint64),
                ('in_kernel', ct.c_uint32),
                ('cpu', ct.c_uint32),
                ('period', ct.c_uint64)]


class EventStruct(ct.Structure):
    _fields_ = [('name', ct.c_char_p)]


class SymbolStruct(ct.Structure):
    _fields_ = [('dso_name', ct.c_char_p),
                ('vaddr_in_file', ct.c_uint64),
                ('symbol_name', ct.c_char_p)]


class CallChainEntryStructure(ct.Structure):
    _fields_ = [('ip', ct.c_uint64),
                ('symbol', SymbolStruct)]


class CallChainStructure(ct.Structure):
    _fields_ = [('nr', ct.c_uint32),
                ('entries', ct.POINTER(CallChainEntryStructure))]

class ReportLibStructure(ct.Structure):
    _fields_ = []


class ReportLib(object):

    def __init__(self, native_lib_path=None):
        if native_lib_path is None:
            native_lib_path = _get_native_lib()

        self._load_dependent_lib()
        self._lib = ct.CDLL(native_lib_path)
        self._CreateReportLibFunc = self._lib.CreateReportLib
        self._CreateReportLibFunc.restype = ct.POINTER(ReportLibStructure)
        self._DestroyReportLibFunc = self._lib.DestroyReportLib
        self._SetLogSeverityFunc = self._lib.SetLogSeverity
        self._SetSymfsFunc = self._lib.SetSymfs
        self._SetRecordFileFunc = self._lib.SetRecordFile
        self._SetKallsymsFileFunc = self._lib.SetKallsymsFile
        self._ShowIpForUnknownSymbolFunc = self._lib.ShowIpForUnknownSymbol
        self._GetNextSampleFunc = self._lib.GetNextSample
        self._GetNextSampleFunc.restype = ct.POINTER(SampleStruct)
        self._GetEventOfCurrentSampleFunc = self._lib.GetEventOfCurrentSample
        self._GetEventOfCurrentSampleFunc.restype = ct.POINTER(EventStruct)
        self._GetSymbolOfCurrentSampleFunc = self._lib.GetSymbolOfCurrentSample
        self._GetSymbolOfCurrentSampleFunc.restype = ct.POINTER(SymbolStruct)
        self._GetCallChainOfCurrentSampleFunc = self._lib.GetCallChainOfCurrentSample
        self._GetCallChainOfCurrentSampleFunc.restype = ct.POINTER(
            CallChainStructure)
        self._instance = self._CreateReportLibFunc()
        assert(not _is_null(self._instance))

    def _load_dependent_lib(self):
        # As the windows dll is built with mingw we need to also find "libwinpthread-1.dll".
        # Load it before libsimpleperf_report.dll if it does exist in the same folder as this script.
        if _isWindows():
            libwinpthread_path = os.path.join(_get_script_path(), "libwinpthread-1.dll")
            if os.path.exists(libwinpthread_path):
                self._libwinpthread = ct.CDLL(libwinpthread_path)

    def Close(self):
        if self._instance is None:
            return
        self._DestroyReportLibFunc(self._instance)
        self._instance = None

    def SetLogSeverity(self, log_level='info'):
        """ Set log severity of native lib, can be verbose,debug,info,error,fatal."""
        cond = self._SetLogSeverityFunc(self.getInstance(), _char_pt(log_level))
        self._check(cond, "Failed to set log level")

    def SetSymfs(self, symfs_dir):
        """ Set directory used to find symbols."""
        cond = self._SetSymfsFunc(self.getInstance(), _char_pt(symfs_dir))
        self._check(cond, "Failed to set symbols directory")

    def SetRecordFile(self, record_file):
        """ Set the path of record file, like perf.data."""
        cond = self._SetRecordFileFunc(self.getInstance(), _char_pt(record_file))
        self._check(cond, "Failed to set record file")

    def ShowIpForUnknownSymbol(self):
        self._ShowIpForUnknownSymbolFunc(self.getInstance())

    def SetKallsymsFile(self, kallsym_file):
        """ Set the file path to a copy of the /proc/kallsyms file (for off device decoding) """
        cond = self._SetKallsymsFileFunc(self.getInstance(), _char_pt(kallsym_file))
        self._check(cond, "Failed to set kallsyms file")

    def GetNextSample(self):
        sample = self._GetNextSampleFunc(self.getInstance())
        if _is_null(sample):
            return None
        return sample

    def GetEventOfCurrentSample(self):
        event = self._GetEventOfCurrentSampleFunc(self.getInstance())
        assert(not _is_null(event))
        return event

    def GetSymbolOfCurrentSample(self):
        symbol = self._GetSymbolOfCurrentSampleFunc(self.getInstance())
        assert(not _is_null(symbol))
        return symbol

    def GetCallChainOfCurrentSample(self):
        callchain = self._GetCallChainOfCurrentSampleFunc(self.getInstance())
        assert(not _is_null(callchain))
        return callchain

    def getInstance(self):
        if self._instance is None:
            raise Exception("Instance is Closed")
        return self._instance

    def _check(self, cond, failmsg):
        if not cond:
            raise Exception(failmsg)
