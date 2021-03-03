//
// Copyright (C) 2021 The Android Open Source Project
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

//! ProfCollect configurations.

use anyhow::Result;
use lazy_static::lazy_static;
use serde::{Deserialize, Serialize};
use std::error::Error;
use std::path::Path;
use std::str::FromStr;
use std::time::Duration;

const PROFCOLLECT_CONFIG_NAMESPACE: &str = "profcollect_native_boot";

lazy_static! {
    pub static ref TRACE_OUTPUT_DIR: &'static Path = Path::new("/data/misc/profcollectd/trace/");
    pub static ref PROFILE_OUTPUT_DIR: &'static Path = Path::new("/data/misc/profcollectd/output/");
    pub static ref REPORT_OUTPUT_DIR: &'static Path = Path::new("/data/misc/profcollectd/report/");
    pub static ref CONFIG_FILE: &'static Path =
        Path::new("/data/misc/profcollectd/output/config.json");
}

#[derive(Clone, PartialEq, Eq, Serialize, Deserialize, Debug)]
pub struct Config {
    /// Version of config file scheme, always equals to 1.
    version: u32,
    /// Device build fingerprint.
    pub build_fingerprint: String,
    /// Interval between collections.
    pub collection_interval: Duration,
    /// Length of time each collection lasts for.
    pub sampling_period: Duration,
    /// An optional filter to limit which binaries to or not to profile.
    pub binary_filter: String,
}

impl Config {
    pub fn from_env() -> Result<Self> {
        Ok(Config {
            version: 1,
            build_fingerprint: get_build_fingerprint(),
            collection_interval: Duration::from_secs(get_device_config(
                "collection_interval",
                600,
            )?),
            sampling_period: Duration::from_millis(get_device_config("sampling_period", 500)?),
            binary_filter: get_device_config("binary_filter", "".to_string())?,
        })
    }
}

impl ToString for Config {
    fn to_string(&self) -> String {
        serde_json::to_string(self).expect("Failed to deserialise configuration.")
    }
}

impl FromStr for Config {
    type Err = serde_json::Error;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        serde_json::from_str::<Config>(s)
    }
}

fn get_build_fingerprint() -> String {
    profcollect_libbase_rust::get_property("ro.build.fingerprint", "unknown").to_string()
}

fn get_device_config<T>(key: &str, default: T) -> Result<T>
where
    T: FromStr + ToString,
    T::Err: Error + Send + Sync + 'static,
{
    let default = default.to_string();
    let config = profcollect_libflags_rust::get_server_configurable_flag(
        &PROFCOLLECT_CONFIG_NAMESPACE,
        &key,
        &default,
    );
    Ok(T::from_str(&config)?)
}
