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
LOCAL_PATH := $(call my-dir)

SIMPLEPERF_SCRIPT_LIST := annotate.config annotate.py app_profiler.config \
                          app_profiler.py binary_cache_builder.config \
                          binary_cache_builder.py report_sample.py \
                          report.py simpleperf_report_lib.py utils.py \
                          ../README.md

SIMPLEPERF_SCRIPT_LIST := $(addprefix $(LOCAL_PATH)/, $(SIMPLEPERF_SCRIPT_LIST))

$(HOST_OUT_EXECUTABLES)/simpleperf_script.zip : $(SIMPLEPERF_SCRIPT_LIST)
	zip -j - $^ >$@

SIMPLEPERF_SCRIPT_LIST :=

sdk: $(HOST_OUT_EXECUTABLES)/simpleperf_script.zip

$(call dist-for-goals,sdk,$(HOST_OUT_EXECUTABLES)/simpleperf_script.zip)