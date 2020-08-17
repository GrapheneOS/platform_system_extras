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

//! Command to control profcollectd behaviour.

use std::env;

fn print_help() {
    println!(
        r#"(
usage: profcollectctl [command]

Command to control profcollectd behaviour.

command:
    start       Schedule periodic collection.
    stop        Terminate periodic collection.
    once        Request an one-off trace.
    process     Convert traces to perf profiles.
    reconfig    Refresh configuration.
    help        Print this message.
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
        "start" => {
            println!("Scheduling profile collection");
            libprofcollectd::schedule_collection();
        }
        "stop" => {
            println!("Terminating profile collection");
            libprofcollectd::terminate_collection();
        }
        "once" => {
            println!("Trace once");
            libprofcollectd::trace_once();
        }
        "process" => {
            println!("Processing traces");
            libprofcollectd::process();
        }
        "reconfig" => {
            println!("Refreshing configuration");
            libprofcollectd::read_config();
        }
        "help" => print_help(),
        _ => {
            print_help();
            std::process::exit(1);
        }
    }
}
