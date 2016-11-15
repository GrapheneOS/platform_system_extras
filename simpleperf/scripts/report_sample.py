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

import sys
from simpleperf_report_lib import *


def usage():
    print 'python report_sample.py [options] <record_file>'
    print '-h/--help print this help message'
    print '--symfs <symfs_dir>  Set the path to looking for symbols'
    print 'If record file is not given, use default file perf.data.'


def report_sample(record_file, symfs_dir):
    """ read record_file, and print each sample"""
    lib = ReportLib()

    lib.ShowIpForUnknownSymbol()
    if symfs_dir is not None:
        lib.SetSymfs(symfs_dir)
    if record_file is not None:
        lib.SetRecordFile(record_file)

    while True:
        sample = lib.GetNextSample()
        if sample is None:
            lib.Close()
            break
        event = lib.GetEventOfCurrentSample()
        symbol = lib.GetSymbolOfCurrentSample()
        callchain = lib.GetCallChainOfCurrentSample()

        sec = sample[0].time / 1000000000
        usec = (sample[0].time - sec * 1000000000) / 1000
        print '%s\t%d [%03d] %d.%d:\t\t%d %s:' % (sample[0].thread_comm, sample[0].tid, sample[0].cpu, sec, usec, sample[0].period, event[0].name)
        print '%16x\t%s (%s)' % (sample[0].ip, symbol[0].symbol_name, symbol[0].dso_name)
        for i in range(callchain[0].nr):
            entry = callchain[0].entries[i]
            print '%16x\t%s (%s)' % (entry.ip, entry.symbol.symbol_name, entry.symbol.dso_name)
        print


if __name__ == '__main__':
    record_file = 'perf.data'
    symfs_dir = None
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
                print 'argument for --symfs is missing'
                sys.exit(1)
        else:
          record_file = sys.argv[i]
        i += 1

    report_sample(record_file, symfs_dir)
