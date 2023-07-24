// Copyright (C) 2023 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! Usage sample for libatrace_rust.

use std::thread::JoinHandle;

use atrace::AtraceTag;

fn spawn_async_event() -> JoinHandle<()> {
    atrace::atrace_async_begin(AtraceTag::App, "Async task", 12345);
    std::thread::spawn(|| {
        std::thread::sleep(std::time::Duration::from_millis(500));
        atrace::atrace_async_end(AtraceTag::App, "Async task", 12345);
    })
}

fn spawn_async_event_with_track() -> JoinHandle<()> {
    atrace::atrace_async_for_track_begin(AtraceTag::App, "Async track", "Task with track", 12345);
    std::thread::spawn(|| {
        std::thread::sleep(std::time::Duration::from_millis(600));
        atrace::atrace_async_for_track_end(AtraceTag::App, "Async track", 12345);
    })
}

fn main() {
    let enabled_tags = atrace::atrace_get_enabled_tags();
    println!("Enabled tags: {:?}", enabled_tags);

    println!("Spawning async trace events");
    let async_event_handler = spawn_async_event();
    let async_event_with_track_handler = spawn_async_event_with_track();

    println!("Calling atrace_begin and sleeping for 1 sec...");
    atrace::atrace_begin(AtraceTag::App, "Hello tracing!");
    std::thread::sleep(std::time::Duration::from_secs(1));
    atrace::atrace_end(AtraceTag::App);

    println!("Joining async events...");
    async_event_handler.join().unwrap();
    async_event_with_track_handler.join().unwrap();

    println!("Done!");
}
