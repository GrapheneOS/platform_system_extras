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

#include "perfprofd_perf.h"


#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "config.h"

namespace android {
namespace perfprofd {

//
// Invoke "perf record". Return value is OK_PROFILE_COLLECTION for
// success, or some other error code if something went wrong.
//
PerfResult InvokePerf(Config& config,
                      const std::string &perf_path,
                      const char *stack_profile_opt,
                      unsigned duration,
                      const std::string &data_file_path,
                      const std::string &perf_stderr_path)
{
  pid_t pid = fork();

  if (pid == -1) {
    PLOG(ERROR) << "Fork failed";
    return PerfResult::kForkFailed;
  }

  if (pid == 0) {
    // child

    // Open file to receive stderr/stdout from perf
    FILE *efp = fopen(perf_stderr_path.c_str(), "w");
    if (efp) {
      dup2(fileno(efp), STDERR_FILENO);
      dup2(fileno(efp), STDOUT_FILENO);
    } else {
      PLOG(WARNING) << "unable to open " << perf_stderr_path << " for writing";
    }

    // marshall arguments
    constexpr unsigned max_args = 17;
    const char *argv[max_args];
    unsigned slot = 0;
    argv[slot++] = perf_path.c_str();
    argv[slot++] = "record";

    // -o perf.data
    argv[slot++] = "-o";
    argv[slot++] = data_file_path.c_str();

    // -c/f N
    std::string p_str;
    if (config.sampling_frequency > 0) {
      argv[slot++] = "-f";
      p_str = android::base::StringPrintf("%u", config.sampling_frequency);
      argv[slot++] = p_str.c_str();
    } else if (config.sampling_period > 0) {
      argv[slot++] = "-c";
      p_str = android::base::StringPrintf("%u", config.sampling_period);
      argv[slot++] = p_str.c_str();
    }

    // -g if desired
    if (stack_profile_opt) {
      argv[slot++] = stack_profile_opt;
      argv[slot++] = "-m";
      argv[slot++] = "8192";
    }

    std::string pid_str;
    if (config.process < 0) {
      // system wide profiling
      argv[slot++] = "-a";
    } else {
      argv[slot++] = "-p";
      pid_str = std::to_string(config.process);
      argv[slot++] = pid_str.c_str();
    }

    // no need for kernel or other symbols
    argv[slot++] = "--no-dump-kernel-symbols";
    argv[slot++] = "--no-dump-symbols";

    // sleep <duration>
    argv[slot++] = "--duration";
    std::string d_str = android::base::StringPrintf("%u", duration);
    argv[slot++] = d_str.c_str();

    // terminator
    argv[slot++] = nullptr;
    assert(slot < max_args);

    // record the final command line in the error output file for
    // posterity/debugging purposes
    fprintf(stderr, "perf invocation (pid=%d):\n", getpid());
    for (unsigned i = 0; argv[i] != nullptr; ++i) {
      fprintf(stderr, "%s%s", i ? " " : "", argv[i]);
    }
    fprintf(stderr, "\n");

    // exec
    execvp(argv[0], (char * const *)argv);
    fprintf(stderr, "exec failed: %s\n", strerror(errno));
    exit(1);

  } else {
    // parent

    // Try to sleep.
    config.Sleep(duration);

    // We may have been woken up to stop profiling.
    if (config.ShouldStopProfiling()) {
      // Send SIGHUP to simpleperf to make it stop.
      kill(pid, SIGHUP);
    }

    // Wait for the child, so it's reaped correctly.
    int st = 0;
    pid_t reaped = TEMP_FAILURE_RETRY(waitpid(pid, &st, 0));

    auto print_perferr = [&perf_stderr_path]() {
      std::string tmp;
      if (android::base::ReadFileToString(perf_stderr_path, &tmp)) {
        LOG(WARNING) << tmp;
      } else {
        PLOG(WARNING) << "Could not read " << perf_stderr_path;
      }
    };

    if (reaped == -1) {
      PLOG(WARNING) << "waitpid failed";
    } else if (WIFSIGNALED(st)) {
      if (WTERMSIG(st) == SIGHUP && config.ShouldStopProfiling()) {
        // That was us...
        return PerfResult::kOK;
      }
      LOG(WARNING) << "perf killed by signal " << WTERMSIG(st);
      print_perferr();
    } else if (WEXITSTATUS(st) != 0) {
      LOG(WARNING) << "perf bad exit status " << WEXITSTATUS(st);
      print_perferr();
    } else {
      return PerfResult::kOK;
    }
  }

  return PerfResult::kRecordFailed;
}

}  // namespace perfprofd
}  // namespace android
