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


def _get_script_path():
    return os.path.dirname(os.path.realpath(__file__))


def _is_null(p):
    return ct.cast(p, ct.c_void_p).value is None


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


class ReportLib(object):

    def __init__(self, native_lib_path=None):
        if native_lib_path is None:
            native_lib_path = _get_script_path() + "/libsimpleperf_report.so"
        self._lib = ct.CDLL(native_lib_path)
        self._SetLogSeverityFunc = self._lib.SetLogSeverity
        self._SetSymfsFunc = self._lib.SetSymfs
        self._SetRecordFileFunc = self._lib.SetRecordFile
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

    def SetLogSeverity(self, log_level='info'):
        """ Set log severity of native lib, can be verbose,debug,info,error,fatal."""
        assert(self._SetLogSeverityFunc(log_level))

    def SetSymfs(self, symfs_dir):
        """ Set directory used to find symbols."""
        assert(self._SetSymfsFunc(symfs_dir))

    def SetRecordFile(self, record_file):
        """ Set the path of record file, like perf.data."""
        assert(self._SetRecordFileFunc(record_file))

    def ShowIpForUnknownSymbol(self):
        self._ShowIpForUnknownSymbolFunc()

    def GetNextSample(self):
        sample = self._GetNextSampleFunc()
        if _is_null(sample):
            return None
        return sample

    def GetEventOfCurrentSample(self):
        event = self._GetEventOfCurrentSampleFunc()
        assert(not _is_null(event))
        return event

    def GetSymbolOfCurrentSample(self):
        symbol = self._GetSymbolOfCurrentSampleFunc()
        assert(not _is_null(symbol))
        return symbol

    def GetCallChainOfCurrentSample(self):
        callchain = self._GetCallChainOfCurrentSampleFunc()
        assert(not _is_null(callchain))
        return callchain
