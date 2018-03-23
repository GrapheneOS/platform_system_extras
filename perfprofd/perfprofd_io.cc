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

#include <android-base/file.h>
#include <android-base/logging.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include "perfprofd_record.pb.h"

namespace android {
namespace perfprofd {

using android::base::unique_fd;
using android::base::WriteFully;

namespace {

// Protobuf's file implementation is not available in protobuf-lite. :-(
class FileCopyingOutputStream : public ::google::protobuf::io::CopyingOutputStream {
 public:
  explicit FileCopyingOutputStream(android::base::unique_fd&& fd_in) : fd_(std::move(fd_in)) {
  };
  bool Write(const void * buffer, int size) override {
    return WriteFully(fd_.get(), buffer, size);
  }

 private:
  android::base::unique_fd fd_;
};

}  // namespace

bool SerializeProtobuf(android::perfprofd::PerfprofdRecord* encodedProfile,
                       android::base::unique_fd&& fd) {
  FileCopyingOutputStream fcos(std::move(fd));
  google::protobuf::io::CopyingOutputStreamAdaptor cosa(&fcos);

  bool serialized = encodedProfile->SerializeToZeroCopyStream(&cosa);
  if (!serialized) {
    LOG(WARNING) << "SerializeToZeroCopyStream failed";
    return false;
  }

  cosa.Flush();
  return true;
}

bool SerializeProtobuf(PerfprofdRecord* encodedProfile, const char* encoded_file_path) {
  unlink(encoded_file_path);  // Attempt to unlink for a clean slate.
  constexpr int kFlags = O_CREAT | O_WRONLY | O_TRUNC | O_NOFOLLOW | O_CLOEXEC;
  unique_fd fd(open(encoded_file_path, kFlags, 0664));
  if (fd.get() == -1) {
    PLOG(WARNING) << "Could not open " << encoded_file_path << " for serialization";
    return false;
  }
  return SerializeProtobuf(encodedProfile, std::move(fd));
}

}  // namespace perfprofd
}  // namespace android
