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

import logging
import subprocess

logging.basicConfig(format='%(message)s')

class SimpleperfEvents:
    def __init__(self, reg, cats):
        self.categories = cats
        self.regular = reg

def parse_simpleperf_events(str):
    regular = []
    tracepoints = []
    scan_tracepoints = False

    for line in str.splitlines():
        if line == 'List of tracepoint events:':
            scan_tracepoints = True
        elif not line.startswith('  '):
            scan_tracepoints = False

        if line.startswith('  '):
            # Trim the line, including comments.
            comment_index = line.find('#')
            if comment_index >= 0:
                line = line[:comment_index]
            line = line.strip()
            if line is not '' and line != 'inplace-sampler':
                (tracepoints if scan_tracepoints else regular).append(line)

    subcategories = {}
    for event in tracepoints:
        colon_index = event.find(':')
        if colon_index > 0:
            cat = event[:colon_index]
            name = event[colon_index+1:]
            if cat not in subcategories:
                subcategories[cat] = []
            subcategories[cat].append(name)
        else:
            print 'Warning: unrecognized tracepoint %s' % (event)

    return SimpleperfEvents(regular, subcategories)

events = parse_simpleperf_events(subprocess.check_output(['adb', 'shell', 'simpleperf list']))

field_count = 1

print """
option java_package = "android.perfprofd";

package android.perfprofd;

message CounterSet {
"""

for event in events.regular:
    print '  optional bool %s = %d;' % (event.replace('-', '_'), field_count)
    field_count += 1

print ''

print """
  message TracepointSet {
"""

def FirstUpper(str):
    return str[0:1].upper() + str[1:]

cat_count = 1;

for cat in sorted(events.categories):
    print """
    message %s {
""" % (FirstUpper(cat.replace('-', '_')))

    cat_field_count = 1
    for name in events.categories[cat]:
        print "      optional bool %s = %d;" % (name.replace('-', '_'), cat_field_count)
        cat_field_count += 1

    print """
    };
    optional %s %s = %d;
""" % (FirstUpper(cat.replace('-', '_')), cat.replace('-', '_'), cat_count)
    cat_count += 1

print """
  };
  optional TracePointSet tracepoints = %d;
};

message PerfConfigElement {
  optional CounterSet counter_set = 1;
  optional bool as_group = 2 [ default = false ];
  optional uint32 sampling_period = 3;
};

""" % (field_count)

# Generate C code for names.

print """
std::vector<const char*> GetEvents(const ::android::perfprofd::CounterSet& counter_set) {
  std::vector<const char*> result;
"""

for event in events.regular:
    proto_name = event.replace('-', '_').lower();
    print '  if (counter_set.has_%s() && counter_set.%s())' % (proto_name, proto_name)
    print '    result.push_back("%s");' % (event)

print """
  if (counter_set.has_tracepoints()) {
    const auto& tracepoints = counter_set.tracepoints();
"""

for cat in sorted(events.categories):
    cat_proto_name = cat.replace('-', '_').lower()

    print """
    if (tracepoints.has_%s()) {
      const auto& %s = tracepoints.%s();
""" % (cat_proto_name, cat_proto_name, cat_proto_name)

    for name in events.categories[cat]:
        name_proto_name = name.replace('-', '_').lower()
        print '      if (%s.has_%s() && %s.%s())' % (cat_proto_name, name_proto_name, cat_proto_name, name_proto_name)
        print '        result.push_back("%s:%s");' % (cat, name)

    print '    }'

print """
  }

  return result;
}
"""