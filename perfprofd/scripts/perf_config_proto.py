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

# Test converter of a Config proto.

# Generate with:
#  aprotoc -I=system/extras/perfprofd --python_out=system/extras/perfprofd/scripts \
#      system/extras/perfprofd/perfprofd_config.proto
import perfprofd_config_pb2

import sys

config = perfprofd_config_pb2.ProfilingConfig()

config.collection_interval_in_s = 10
config.sample_duration_in_s = 5
config.main_loop_iterations = 1
config.process = 784

f = open(sys.argv[1], "wb")
f.write(config.SerializeToString())
f.close()
