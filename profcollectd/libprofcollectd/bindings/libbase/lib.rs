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

//! This module implements safe wrappers for GetProperty method from libbase.

use std::ffi::{CStr, CString};

/// Returns the current value of the system property `key`,
/// or `default_value` if the property is empty or doesn't exist.
pub fn get_property<'a>(key: &str, default_value: &'a str) -> &'a str {
    let key = CString::new(key).unwrap();
    let default_value = CString::new(default_value).unwrap();
    unsafe {
        let cstr = profcollect_libbase_bindgen::GetProperty(key.as_ptr(), default_value.as_ptr());
        CStr::from_ptr(cstr).to_str().unwrap()
    }
}

/// Sets the system property `key` to `value`.
pub fn set_property(key: &str, value: &str) {
    let key = CString::new(key).unwrap();
    let value = CString::new(value).unwrap();
    unsafe {
        profcollect_libbase_bindgen::SetProperty(key.as_ptr(), value.as_ptr());
    }
}
