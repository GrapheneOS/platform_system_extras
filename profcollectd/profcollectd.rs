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

//! Daemon program to collect system traces.

use std::env;

fn print_help() {
    println!(
        r#"(
usage: profcollectd [command]
    boot      Start daemon and schedule profile collection after a short delay.
    run       Start daemon but do not schedule profile collection.
)"#
    );
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        print_help();
        std::process::exit(1);
    }

    let action = &args[1];
    match action.as_str() {
        "boot" => libprofcollectd::init_service(true),
        "run" => libprofcollectd::init_service(false),
        "help" => print_help(),
        _ => {
            print_help();
            std::process::exit(1);
        }
    }
}
