#!/usr/bin/python
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

# Super simplistic printer of a perfprofd output proto. Illustrates
# how to parse and traverse a perfprofd output proto in Python.

# Generate with:
#  aprotoc -I=system/extras/perfprofd --python_out=system/extras/perfprofd/scripts \
#      system/extras/perfprofd/perf_profile.proto
import perf_profile_pb2

# Make sure that symbol is on the PYTHONPATH, e.g., run as
# PYTHONPATH=$PYTHONPATH:$ANDROID_BUILD_TOP/development/scripts python ...
import symbol

# This is wrong. But then the symbol module is a bad quagmire.
symbol.SetAbi(["ABI: 'arm64'"])
print "Reading symbols from", symbol.SYMBOLS_DIR

# TODO: accept argument for parsing.
file = open('perf.data.encoded.0', 'rb')
data = file.read()

profile = perf_profile_pb2.AndroidPerfProfile()
profile.ParseFromString(data)

print "Total samples: ", profile.total_samples

module_list = profile.load_modules

counters = {}

def indent(txt, stops = 1):
    return '\n'.join('  ' * stops + line for line in txt.splitlines())


def print_samples(module_list, programs, counters):
    print 'Samples:'
    for program in programs:
        print indent(program.name, 1)
        for module in program.modules:
            if module.HasField('load_module_id'):
                module_descr = module_list[module.load_module_id]
                print indent(module_descr.name, 2)
                has_build_id = module_descr.HasField('build_id')
                if has_build_id:
                    print indent('Build ID: %s' % (module_descr.build_id), 3)
                for addr in module.address_samples:
                    # TODO: Stacks vs single samples.
                    addr_rel = addr.address[0]
                    addr_rel_hex = "%x" % addr_rel
                    print indent('%d %s' % (addr.count, addr_rel_hex), 3)
                    if module_descr.name != '[kernel.kallsyms]':
                        if has_build_id:
                            info = symbol.SymbolInformation(module_descr.name, addr_rel_hex)
                            # As-is, only info[0] (inner-most inlined function) is recognized.
                            (source_symbol, source_location, object_symbol_with_offset) = info[0]
                            if object_symbol_with_offset is not None:
                                print indent(object_symbol_with_offset, 4)
                            if source_symbol is not None:
                                for (sym_inlined, loc_inlined, _) in info:
                                    print indent(sym_inlined, 5)
                                    if loc_inlined is not None:
                                        print ' %s' % (indent(loc_inlined, 5))
                        elif module_descr.symbol and (addr_rel & 0x8000000000000000 != 0):
                            index = 0xffffffffffffffff - addr_rel
                            source_symbol = module_descr.symbol[index]
                            print indent(source_symbol, 4)
                        counters_key = None
                        if source_symbol is not None:
                            counters_key = (module_descr.name, source_symbol)
                        else:
                            counters_key = (module_descr.name, addr_rel_hex)
                        if counters_key in counters:
                            counters[counters_key] = counters[counters_key] + addr.count
                        else:
                            counters[counters_key] = addr.count
            else:
                print indent('<Missing module>', 2)

def print_histogram(counters, size):
    # Create a sorted list of top samples.
    counter_list = []
    for key, value in counters.iteritems():
        temp = (key,value)
        counter_list.append(temp)
    counter_list.sort(key=lambda counter: counter[1], reverse=True)

    # Print top-size samples.
    print 'Histogram top-%d:' % (size)
    for i in xrange(0, min(len(counter_list), size)):
        print indent('%d: %s' % (i+1, counter_list[i]), 1)

def print_modules(module_list):
    print 'Modules:'
    for module in module_list:
        print indent(module.name, 1)
        if module.HasField('build_id'):
            print indent('Build ID: %s' % (module.build_id), 2)
        print indent('Symbols:', 2)
        for symbol in module.symbol:
            print indent(symbol, 3)

print_samples(module_list, profile.programs, counters)
print_modules(module_list)
print_histogram(counters, 100)
