/*
**
** Copyright 2015, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include "perfprofd_io.h"

#include <fcntl.h>
#include <unistd.h>

#include <memory>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>

#include "perfprofd_record.pb.h"

namespace android {
namespace perfprofd {

bool SerializeProtobuf(PerfprofdRecord* encodedProfile,
                       const char* encoded_file_path) {
  //
  // Serialize protobuf to array
  //
  size_t size = encodedProfile->ByteSize();
  std::unique_ptr<uint8_t[]> data(new uint8_t[size]);
  encodedProfile->SerializeWithCachedSizesToArray(data.get());

  //
  // Open file and write encoded data to it
  //
  unlink(encoded_file_path);  // Attempt to unlink for a clean slate.
  constexpr int kFlags = O_CREAT | O_WRONLY | O_TRUNC | O_NOFOLLOW | O_CLOEXEC;
  android::base::unique_fd fd(open(encoded_file_path, kFlags, 0664));
  if (fd.get() == -1) {
    PLOG(WARNING) << "Could not open " << encoded_file_path << " for serialization";
    return false;
  }
  if (!android::base::WriteFully(fd.get(), data.get(), size)) {
    PLOG(WARNING) << "Could not write to " << encoded_file_path;
    return false;
  }

  return true;
}

}  // namespace perfprofd
}  // namespace android
