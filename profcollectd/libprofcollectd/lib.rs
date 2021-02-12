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

//! ProfCollect Binder client interface.

use profcollectd_aidl_interface::aidl::com::android::server::profcollect::IProfCollectd;
use profcollectd_aidl_interface::binder;

/// Initialise profcollectd service.
/// * `start` - Immediately schedule collection after service is initialised.
pub fn init_service(start: bool) {
    unsafe {
        profcollectd_bindgen::android_profcollectd_InitService(start);
    }
}

fn get_profcollectd_service() -> binder::Strong<dyn IProfCollectd::IProfCollectd> {
    let service_name = "profcollectd";
    binder::get_interface(&service_name).expect("could not get profcollectd binder service")
}

/// Schedule periodic profile collection.
pub fn schedule_collection() {
    let service = get_profcollectd_service();
    service.ScheduleCollection().unwrap();
}

/// Terminate periodic profile collection.
pub fn terminate_collection() {
    let service = get_profcollectd_service();
    service.TerminateCollection().unwrap();
}

/// Immediately schedule a one-off trace.
pub fn trace_once() {
    let service = get_profcollectd_service();
    service.TraceOnce("manual").unwrap();
}

/// Process the profiles.
pub fn process() {
    let service = get_profcollectd_service();
    service.ProcessProfile().unwrap();
}

/// Create profile report.
pub fn create_profile_report() {
    let service = get_profcollectd_service();
    service.CreateProfileReport().unwrap();
}

/// Read configs from environment variables.
pub fn read_config() {
    let service = get_profcollectd_service();
    service.ReadConfig().unwrap();
}
