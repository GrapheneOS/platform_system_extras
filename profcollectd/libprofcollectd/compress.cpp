//
// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "compress.h"

#include <ziparchive/zip_writer.h>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace android {
namespace profcollectd {

bool CompressFiles(const std::filesystem::path& output,
                   std::vector<std::filesystem::path>& inputFiles) {
  FILE* outputFile = std::fopen(output.c_str(), "wb");
  ZipWriter writer(outputFile);

  for (const auto& f : inputFiles) {
    // read profile into memory.
    std::ifstream ifs(f, std::ios::in | std::ios::binary);
    std::vector<char> buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    // append to zip file
    writer.StartEntry(f.filename().string(), ZipWriter::kCompress | ZipWriter::kAlign32);
    writer.WriteBytes(buf.data(), buf.size());
    writer.FinishEntry();
  }

  auto ret = writer.Finish();
  std::fclose(outputFile);
  return ret == 0;
}

}  // namespace profcollectd
}  // namespace android
