#ifndef WIRELESS_ANDROID_LOGGING_AWP_PERF_DATA_CONVERTER_H_
#define WIRELESS_ANDROID_LOGGING_AWP_PERF_DATA_CONVERTER_H_

#include <string>

#include "perf_profile.pb.h"

namespace perfprofd {
struct Symbolizer;
}

namespace wireless_android_logging_awp {

wireless_android_play_playlog::AndroidPerfProfile
RawPerfDataToAndroidPerfProfile(const std::string &perf_file,
                                ::perfprofd::Symbolizer* symbolizer);

}  // namespace wireless_android_logging_awp

#endif  // WIRELESS_ANDROID_LOGGING_AWP_PERF_DATA_CONVERTER_H_
