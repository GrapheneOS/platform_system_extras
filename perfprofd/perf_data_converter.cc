
#include "perf_data_converter.h"

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <unordered_map>

#include <android-base/macros.h>
#include <android-base/strings.h>
#include <perf_parser.h>
#include <perf_protobuf_io.h>

#include "perfprofd_record.pb.h"

#include "symbolizer.h"

using std::map;

namespace android {
namespace perfprofd {

PerfprofdRecord*
RawPerfDataToAndroidPerfProfile(const string &perf_file,
                                ::perfprofd::Symbolizer* symbolizer ATTRIBUTE_UNUSED) {
  std::unique_ptr<PerfprofdRecord> ret(new PerfprofdRecord());
  ret->set_id(0);  // TODO.

  quipper::PerfParserOptions options = {};
  options.do_remap = true;
  options.discard_unused_events = true;
  options.read_missing_buildids = true;

  quipper::PerfDataProto* perf_data = ret->mutable_perf_data();

  if (!quipper::SerializeFromFileWithOptions(perf_file, options, perf_data)) {
    return nullptr;
  }

  // TODO: Symbolization.

  return ret.release();
}

}  // namespace perfprofd
}  // namespace android
