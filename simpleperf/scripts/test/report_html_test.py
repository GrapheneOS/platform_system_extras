#!/usr/bin/env python3
#
# Copyright (C) 2021 The Android Open Source Project
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

import collections
import json
from . test_utils import TestBase, TEST_HELPER


class TestReportHtml(TestBase):
    def test_long_callchain(self):
        self.run_cmd(['report_html.py', '-i',
                      TEST_HELPER.testdata_path('perf_with_long_callchain.data')])

    def test_aggregated_by_thread_name(self):
        # Calculate event_count for each thread name before aggregation.
        event_count_for_thread_name = collections.defaultdict(lambda: 0)
        # use "--min_func_percent 0" to avoid cutting any thread.
        self.run_cmd(['report_html.py', '--min_func_percent', '0', '-i',
                      TEST_HELPER.testdata_path('aggregatable_perf1.data'),
                      TEST_HELPER.testdata_path('aggregatable_perf2.data')])
        record_data = self._load_record_data_in_html('report.html')
        event = record_data['sampleInfo'][0]
        for process in event['processes']:
            for thread in process['threads']:
                thread_name = record_data['threadNames'][str(thread['tid'])]
                event_count_for_thread_name[thread_name] += thread['eventCount']

        # Check event count for each thread after aggregation.
        self.run_cmd(['report_html.py', '--aggregate-by-thread-name',
                      '--min_func_percent', '0', '-i',
                      TEST_HELPER.testdata_path('aggregatable_perf1.data'),
                      TEST_HELPER.testdata_path('aggregatable_perf2.data')])
        record_data = self._load_record_data_in_html('report.html')
        event = record_data['sampleInfo'][0]
        hit_count = 0
        for process in event['processes']:
            for thread in process['threads']:
                thread_name = record_data['threadNames'][str(thread['tid'])]
                self.assertEqual(thread['eventCount'],
                                 event_count_for_thread_name[thread_name])
                hit_count += 1
        self.assertEqual(hit_count, len(event_count_for_thread_name))

    def test_no_empty_process(self):
        """ Test not showing a process having no threads. """
        perf_data = TEST_HELPER.testdata_path('two_process_perf.data')
        self.run_cmd(['report_html.py', '-i', perf_data])
        record_data = self._load_record_data_in_html('report.html')
        processes = record_data['sampleInfo'][0]['processes']
        self.assertEqual(len(processes), 2)

        # One process is removed because all its threads are removed for not
        # reaching the min_func_percent limit.
        self.run_cmd(['report_html.py', '-i', perf_data, '--min_func_percent', '20'])
        record_data = self._load_record_data_in_html('report.html')
        processes = record_data['sampleInfo'][0]['processes']
        self.assertEqual(len(processes), 1)

    def _load_record_data_in_html(self, html_file):
        with open(html_file, 'r') as fh:
            data = fh.read()
        start_str = 'type="application/json"'
        end_str = '</script>'
        start_pos = data.find(start_str)
        self.assertNotEqual(start_pos, -1)
        start_pos = data.find('>', start_pos)
        self.assertNotEqual(start_pos, -1)
        start_pos += 1
        end_pos = data.find(end_str, start_pos)
        self.assertNotEqual(end_pos, -1)
        json_data = data[start_pos:end_pos]
        return json.loads(json_data)
