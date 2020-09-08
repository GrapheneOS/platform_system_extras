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

//! Safe Rust interface over the libprofcollectd_bindgen FFI functions.
//! The methods are safe to call since they do not touch or modify Rust side's
//! memory or threads.

/// Initialise profcollectd service.
/// * `start` - Immediately schedule collection after service is initialised.
pub fn init_service(start: bool) {
    unsafe {
        profcollectd_bindgen::InitService(start);
    }
}

/// Schedule periodic profile collection.
pub fn schedule_collection() {
    unsafe {
        profcollectd_bindgen::ScheduleCollection();
    }
}

/// Terminate periodic profile collection.
pub fn terminate_collection() {
    unsafe {
        profcollectd_bindgen::TerminateCollection();
    }
}

/// Immediately schedule a one-off trace.
pub fn trace_once() {
    unsafe {
        profcollectd_bindgen::TraceOnce();
    }
}

/// Process the profiles.
pub fn process() {
    unsafe {
        profcollectd_bindgen::Process();
    }
}

/// Create profile report.
pub fn create_profile_report() {
    unsafe {
        profcollectd_bindgen::CreateProfileReport();
    }
}

/// Read configs from environment variables.
pub fn read_config() {
    unsafe {
        profcollectd_bindgen::ReadConfig();
    }
}
