#!/usr/bin/env python3

# Copyright (C) 2020 The Android Open Source Project
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

import unittest
from ota_from_raw_image import _GetKernelRelease


class OtaFromRawImageTest(unittest.TestCase):
  def test_get_kernel_release_trivial(self):
    self.assertEqual("5.4.42-android12-15", _GetKernelRelease("5.4.42-android12-15"))

  def test_get_kernel_release_suffix(self):
    self.assertEqual("5.4.42-android12-15", _GetKernelRelease("5.4.42-android12-15-something"))

  def test_get_kernel_release_invalid(self):
    with self.assertRaises(AssertionError):
      _GetKernelRelease("5.4-android12-15")


if __name__ == '__main__':
  # atest needs a verbosity level of >= 2 to correctly parse the result.
  unittest.main(verbosity=2)
