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

#include <stdlib.h>

#include <iostream>

#include "libprofcollectd.h"

namespace profcollectd = android::profcollectd;

namespace {

void PrintHelp(const std::string& reason = "") {
  std::cout << reason;
  std::cout << R"(
usage: profcollectctl [command]
command:
    start       Schedule periodic collection.
    stop        Terminate periodic collection.
    once        Request an one-off trace.
    process     Convert traces to perf profiles.
    reconfig    Refresh configuration.
    help        Print this message.
)";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    PrintHelp("Invalid arguments");
    exit(EXIT_FAILURE);
  }

  std::string command(argv[1]);
  if (command == "start") {
    std::cout << "Scheduling profile collection\n";
    profcollectd::ScheduleCollection();
  } else if (command == "stop") {
    std::cout << "Terminating profile collection\n";
    profcollectd::TerminateCollection();
  } else if (command == "once") {
    std::cout << "Trace once\n";
    profcollectd::TraceOnce();
  } else if (command == "process") {
    std::cout << "Processing traces\n";
    profcollectd::Process();
  } else if (command == "reconfig") {
    std::cout << "Refreshing configuration\n";
    profcollectd::ReadConfig();
  } else if (command == "help") {
    PrintHelp();
  } else {
    PrintHelp("Unknown command: " + command);
    exit(EXIT_FAILURE);
  }
}
