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

"""report_sample.py: report samples in the same format as `perf script`.
"""

from __future__ import print_function
import sys
from simpleperf_report_lib import *


def usage():
    print('python report_sample.py [options] <record_file>')
    print('-h/--help print this help message')
    print('--symfs <symfs_dir>  Set the path to looking for symbols')
    print('--kallsyms <kallsyms_file>  Set the path to a kallsyms file')
    print('If record file is not given, use default file perf.data.')


def report_sample(record_file, symfs_dir, kallsyms_file=None):
    """ read record_file, and print each sample"""
    lib = ReportLib()

    lib.ShowIpForUnknownSymbol()
    if symfs_dir is not None:
        lib.SetSymfs(symfs_dir)
    if record_file is not None:
        lib.SetRecordFile(record_file)
    if kallsyms_file is not None:
        lib.SetKallsymsFile(kallsyms_file)

    while True:
        sample = lib.GetNextSample()
        if sample is None:
            lib.Close()
            break
        event = lib.GetEventOfCurrentSample()
        symbol = lib.GetSymbolOfCurrentSample()
        callchain = lib.GetCallChainOfCurrentSample()

        sec = sample.time / 1000000000
        usec = (sample.time - sec * 1000000000) / 1000
        print('%s\t%d [%03d] %d.%d:\t\t%d %s:' % (sample.thread_comm,
                                                  sample.tid, sample.cpu, sec,
                                                  usec, sample.period, event.name))
        print('%16x\t%s (%s)' % (sample.ip, symbol.symbol_name, symbol.dso_name))
        for i in range(callchain.nr):
            entry = callchain.entries[i]
            print('%16x\t%s (%s)' % (entry.ip, entry.symbol.symbol_name, entry.symbol.dso_name))
        print('')


if __name__ == '__main__':
    record_file = 'perf.data'
    symfs_dir = None
    kallsyms_file = None
    i = 1
    while i < len(sys.argv):
        if sys.argv[i] == '-h' or sys.argv[i] == '--help':
            usage()
            sys.exit(0)
        elif sys.argv[i] == '--symfs':
            if i + 1 < len(sys.argv):
                symfs_dir = sys.argv[i + 1]
                i += 1
            else:
                print('argument for --symfs is missing')
                sys.exit(1)
        elif sys.argv[i] == '--kallsyms':
            if i + 1 < len(sys.argv):
                kallsyms_file = sys.argv[i + 1]
                i += 1
            else:
                print('argument for --kallsyms is missing')
                sys.exit(1)
        else:
          record_file = sys.argv[i]
        i += 1

    report_sample(record_file, symfs_dir, kallsyms_file)
