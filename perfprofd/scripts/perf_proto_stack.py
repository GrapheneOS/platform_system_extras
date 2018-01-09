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

print 'Samples:'
for program in profile.programs:
    print program.name
    for module in program.modules:
        if module.HasField('load_module_id'):
            module_descr = module_list[module.load_module_id]
            print ' ', module_descr.name
            has_build_id = module_descr.HasField('build_id')
            if has_build_id:
                print '   ', module_descr.build_id
            for addr in module.address_samples:
                # TODO: Stacks vs single samples.
                addr_rel = addr.address[0]
                print '     ', addr.count, addr_rel
                if module_descr.name != '[kernel.kallsyms]':
                    addr_rel_hex = "%x" % addr_rel
                    if has_build_id:
                        info = symbol.SymbolInformation(module_descr.name, addr_rel_hex)
                        # As-is, only info[0] (inner-most inlined function) is recognized.
                        (source_symbol, source_location, object_symbol_with_offset) = info[0]
                        if source_symbol is not None:
                            print source_symbol, source_location, object_symbol_with_offset
                    elif module_descr.symbol and (addr_rel & 0x8000000000000000 != 0):
                        index = 0xffffffffffffffff - addr_rel
                        source_symbol = module_descr.symbol[index]
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
            print '  Missing module'

print 'Modules:'
for module in module_list:
    print ' ', module.name
    if module.HasField('build_id'):
        print '   ', module.build_id
    for symbol in module.symbol:
        print '      ', symbol

# Create a sorted list of top samples.
counter_list = []
for key, value in counters.iteritems():
    temp = (key,value)
    counter_list.append(temp)
counter_list.sort(key=lambda counter: counter[1], reverse=True)

# Print top-100 samples.
for i in range(0, 100):
    print i+1, counter_list[i]
