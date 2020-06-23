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

namespace {

void print_help() {
  std::cout << R"(
usage: profcollectd [command]
    boot      Start daemon and schedule profile collection after a short delay.
    run       Start daemon but do not schedule profile collection.
)";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    print_help();
    exit(EXIT_SUCCESS);
  } else if (std::string(argv[1]) == "boot") {
    android::profcollectd::InitService(/* start = */ true);
  } else if (std::string(argv[1]) == "run") {
    android::profcollectd::InitService(/* start = */ false);
  } else {
    print_help();
    exit(EXIT_FAILURE);
  }
}
