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

//! ATrace instrumentation methods from cutils.

use std::ffi::CString;

#[cfg(not(test))]
use cutils_trace_bindgen as trace_bind;

// Wrap tags into a mod to allow missing docs.
// We have to use the mod for this because Rust won't apply the attribute to the bitflags macro
// invocation.
pub use self::tags::*;
pub mod tags {
    // Tag constants are not documented in libcutils, so we don't document them here.
    #![allow(missing_docs)]

    use bitflags::bitflags;
    use static_assertions::const_assert_eq;

    bitflags! {
        /// The trace tag is used to filter tracing in userland to avoid some of the runtime cost of
        /// tracing when it is not desired.
        ///
        /// Using `AtraceTag::Always` will result in the tracing always being enabled - this should
        /// ONLY be done for debug code, as userland tracing has a performance cost even when the
        /// trace is not being recorded. `AtraceTag::Never` will result in the tracing always being
        /// disabled.
        ///
        /// `AtraceTag::Hal` should be bitwise ORed with the relevant tags for tracing
        /// within a hardware module. For example a camera hardware module would use
        /// `AtraceTag::Camera | AtraceTag::Hal`.
        ///
        /// Source of truth is `system/core/libcutils/include/cutils/trace.h`.
        #[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
        pub struct AtraceTag: u64 {
            const Never           = cutils_trace_bindgen::ATRACE_TAG_NEVER as u64;
            const Always          = cutils_trace_bindgen::ATRACE_TAG_ALWAYS as u64;
            const Graphics        = cutils_trace_bindgen::ATRACE_TAG_GRAPHICS as u64;
            const Input           = cutils_trace_bindgen::ATRACE_TAG_INPUT as u64;
            const View            = cutils_trace_bindgen::ATRACE_TAG_VIEW as u64;
            const Webview         = cutils_trace_bindgen::ATRACE_TAG_WEBVIEW as u64;
            const WindowManager   = cutils_trace_bindgen::ATRACE_TAG_WINDOW_MANAGER as u64;
            const ActivityManager = cutils_trace_bindgen::ATRACE_TAG_ACTIVITY_MANAGER as u64;
            const SyncManager     = cutils_trace_bindgen::ATRACE_TAG_SYNC_MANAGER as u64;
            const Audio           = cutils_trace_bindgen::ATRACE_TAG_AUDIO as u64;
            const Video           = cutils_trace_bindgen::ATRACE_TAG_VIDEO as u64;
            const Camera          = cutils_trace_bindgen::ATRACE_TAG_CAMERA as u64;
            const Hal             = cutils_trace_bindgen::ATRACE_TAG_HAL as u64;
            const App             = cutils_trace_bindgen::ATRACE_TAG_APP as u64;
            const Resources       = cutils_trace_bindgen::ATRACE_TAG_RESOURCES as u64;
            const Dalvik          = cutils_trace_bindgen::ATRACE_TAG_DALVIK as u64;
            const Rs              = cutils_trace_bindgen::ATRACE_TAG_RS as u64;
            const Bionic          = cutils_trace_bindgen::ATRACE_TAG_BIONIC as u64;
            const Power           = cutils_trace_bindgen::ATRACE_TAG_POWER as u64;
            const PackageManager  = cutils_trace_bindgen::ATRACE_TAG_PACKAGE_MANAGER as u64;
            const SystemServer    = cutils_trace_bindgen::ATRACE_TAG_SYSTEM_SERVER as u64;
            const Database        = cutils_trace_bindgen::ATRACE_TAG_DATABASE as u64;
            const Network         = cutils_trace_bindgen::ATRACE_TAG_NETWORK as u64;
            const Adb             = cutils_trace_bindgen::ATRACE_TAG_ADB as u64;
            const Vibrator        = cutils_trace_bindgen::ATRACE_TAG_VIBRATOR as u64;
            const Aidl            = cutils_trace_bindgen::ATRACE_TAG_AIDL as u64;
            const Nnapi           = cutils_trace_bindgen::ATRACE_TAG_NNAPI as u64;
            const Rro             = cutils_trace_bindgen::ATRACE_TAG_RRO as u64;
            const Thermal         = cutils_trace_bindgen::ATRACE_TAG_THERMAL as u64;
            const Last            = cutils_trace_bindgen::ATRACE_TAG_LAST as u64;
            const ValidMask       = cutils_trace_bindgen::ATRACE_TAG_VALID_MASK as u64;
        }
    }

    // Assertion to keep tags in sync. If it fails, it means there are new tags added to
    // cutils/trace.h. Add them to the tags above and update the assertion.
    const_assert_eq!(AtraceTag::Thermal.bits(), cutils_trace_bindgen::ATRACE_TAG_LAST as u64);
}

/// Test if a given tag is currently enabled.
///
/// It can be used as a guard condition around more expensive trace calculations.
pub fn atrace_is_tag_enabled(tag: AtraceTag) -> bool {
    // SAFETY: No pointers are transferred.
    unsafe { trace_bind::atrace_is_tag_enabled_wrap(tag.bits()) != 0 }
}

/// Trace the beginning of a context. `name` is used to identify the context.
///
/// This is often used to time function execution.
pub fn atrace_begin(tag: AtraceTag, name: &str) {
    if !atrace_is_tag_enabled(tag) {
        return;
    }

    let name_cstr = CString::new(name.as_bytes()).expect("CString::new failed");
    // SAFETY: The function does not accept the pointer ownership, only reads its contents.
    // The passed string is guaranteed to be null-terminated by CString.
    unsafe {
        trace_bind::atrace_begin_wrap(tag.bits(), name_cstr.as_ptr());
    }
}

/// Trace the end of a context.
///
/// This should match up (and occur after) a corresponding `atrace_begin`.
pub fn atrace_end(tag: AtraceTag) {
    // SAFETY: No pointers are transferred.
    unsafe {
        trace_bind::atrace_end_wrap(tag.bits());
    }
}

#[cfg(test)]
use self::tests::mock_atrace as trace_bind;

#[cfg(test)]
mod tests {
    use super::*;

    use std::ffi::CStr;
    use std::os::raw::c_char;

    /// Utilities to mock ATrace bindings.
    ///
    /// Normally, for behavior-driven testing we focus on the outcomes of the functions rather than
    /// calls into bindings. However, since the purpose of the library is to forward data into
    /// the underlying implementation (which we assume to be correct), that's what we test.
    pub mod mock_atrace {
        use std::cell::RefCell;
        use std::os::raw::c_char;

        /// Contains logic to check binding calls.
        /// Implement this trait in the test with mocking logic and checks in implemented functions.
        /// Default implementations panic.
        pub trait ATraceMocker {
            fn atrace_is_tag_enabled_wrap(&mut self, _tag: u64) -> u64 {
                panic!("Unexpected call");
            }
            fn atrace_begin_wrap(&mut self, _tag: u64, _name: *const c_char) {
                panic!("Unexpected call");
            }
            fn atrace_end_wrap(&mut self, _tag: u64) {
                panic!("Unexpected call");
            }

            /// This method should contain checks to be performed at the end of the test.
            fn finish(&self) {}
        }

        struct DefaultMocker;
        impl ATraceMocker for DefaultMocker {}

        // Global mock object is thread-local, so that the tests can run safely in parallel.
        thread_local!(static MOCKER: RefCell<Box<dyn ATraceMocker>> = RefCell::new(Box::new(DefaultMocker{})));

        /// Sets the global mock object.
        fn set_mocker(mocker: Box<dyn ATraceMocker>) {
            MOCKER.with(|m| *m.borrow_mut() = mocker)
        }

        /// Calls the passed method `f` with a mutable reference to the global mock object.
        /// Example:
        /// ```
        /// with_mocker(|mocker| mocker.atrace_begin_wrap(tag, name))
        /// ```
        fn with_mocker<F, R>(f: F) -> R
        where
            F: FnOnce(&mut dyn ATraceMocker) -> R,
        {
            MOCKER.with(|m| f(m.borrow_mut().as_mut()))
        }

        /// Finish the test and perform final checks in the mocker.
        /// Calls `finish()` on the global mocker.
        ///
        /// Needs to be called manually at the end of each test that uses mocks.
        ///
        /// May panic, so it can not be called in `drop()` methods,
        /// since it may result in double panic.
        pub fn mocker_finish() {
            with_mocker(|m| m.finish())
        }

        /// RAII guard that resets the mock to the default implementation.
        pub struct MockerGuard;
        impl Drop for MockerGuard {
            fn drop(&mut self) {
                set_mocker(Box::new(DefaultMocker {}));
            }
        }

        /// Sets the mock object for the duration of the scope.
        ///
        /// Returns a RAII guard that resets the mock back to default on destruction.
        pub fn set_scoped_mocker<T: ATraceMocker + 'static>(m: T) -> MockerGuard {
            set_mocker(Box::new(m));
            MockerGuard {}
        }

        // Wrapped functions that forward calls into mocker.
        // The functions are marked as unsafe to match the binding interface, won't compile otherwise.
        // The mocker methods themselves are not marked as unsafe.

        pub unsafe fn atrace_is_tag_enabled_wrap(tag: u64) -> u64 {
            with_mocker(|m| m.atrace_is_tag_enabled_wrap(tag))
        }
        pub unsafe fn atrace_begin_wrap(tag: u64, name: *const c_char) {
            with_mocker(|m| m.atrace_begin_wrap(tag, name))
        }
        pub unsafe fn atrace_end_wrap(tag: u64) {
            with_mocker(|m| m.atrace_end_wrap(tag))
        }
    }

    #[test]
    fn forwards_trace_begin() {
        #[derive(Default)]
        struct CallCheck {
            begin_count: u32,
        }

        impl mock_atrace::ATraceMocker for CallCheck {
            fn atrace_is_tag_enabled_wrap(&mut self, _tag: u64) -> u64 {
                1
            }
            fn atrace_begin_wrap(&mut self, tag: u64, name: *const c_char) {
                self.begin_count += 1;
                assert!(self.begin_count < 2);
                assert_eq!(tag, cutils_trace_bindgen::ATRACE_TAG_APP as u64);
                // SAFETY: If the code under test is correct, the pointer is guaranteed to satisfy
                // the requirements of `CStr::from_ptr`. If the code is not correct, this section is
                // unsafe and will hopefully fail the test.
                unsafe {
                    assert_eq!(CStr::from_ptr(name).to_str().expect("to_str failed"), "Test Name");
                }
            }

            fn finish(&self) {
                assert_eq!(self.begin_count, 1);
            }
        }

        let _guard = mock_atrace::set_scoped_mocker(CallCheck::default());

        atrace_begin(AtraceTag::App, "Test Name");

        mock_atrace::mocker_finish();
    }

    #[test]
    fn trace_begin_not_called_with_disabled_tag() {
        #[derive(Default)]
        struct CallCheck {
            is_tag_enabled_count: u32,
        }

        impl mock_atrace::ATraceMocker for CallCheck {
            fn atrace_is_tag_enabled_wrap(&mut self, _tag: u64) -> u64 {
                self.is_tag_enabled_count += 1;
                assert!(self.is_tag_enabled_count < 2);
                0
            }
            fn atrace_begin_wrap(&mut self, _tag: u64, _name: *const c_char) {
                panic!("Begin should not be called with disabled tag.")
            }

            fn finish(&self) {
                assert_eq!(self.is_tag_enabled_count, 1);
            }
        }

        let _guard = mock_atrace::set_scoped_mocker(CallCheck::default());

        atrace_begin(AtraceTag::App, "Ignore me");

        mock_atrace::mocker_finish();
    }

    #[test]
    fn forwards_trace_end() {
        #[derive(Default)]
        struct CallCheck {
            end_count: u32,
        }

        impl mock_atrace::ATraceMocker for CallCheck {
            fn atrace_end_wrap(&mut self, tag: u64) {
                self.end_count += 1;
                assert!(self.end_count < 2);
                assert_eq!(tag, cutils_trace_bindgen::ATRACE_TAG_APP as u64);
            }

            fn finish(&self) {
                assert_eq!(self.end_count, 1);
            }
        }

        let _guard = mock_atrace::set_scoped_mocker(CallCheck::default());

        atrace_end(AtraceTag::App);

        mock_atrace::mocker_finish();
    }

    #[test]
    fn can_combine_tags() {
        #[derive(Default)]
        struct CallCheck {
            begin_count: u32,
        }

        impl mock_atrace::ATraceMocker for CallCheck {
            fn atrace_is_tag_enabled_wrap(&mut self, _tag: u64) -> u64 {
                1
            }
            fn atrace_begin_wrap(&mut self, tag: u64, _name: *const c_char) {
                self.begin_count += 1;
                assert!(self.begin_count < 2);
                assert_eq!(
                    tag,
                    (cutils_trace_bindgen::ATRACE_TAG_HAL | cutils_trace_bindgen::ATRACE_TAG_CAMERA)
                        as u64
                );
            }

            fn finish(&self) {
                assert_eq!(self.begin_count, 1);
            }
        }

        let _guard = mock_atrace::set_scoped_mocker(CallCheck::default());

        atrace_begin(AtraceTag::Hal | AtraceTag::Camera, "foo");

        mock_atrace::mocker_finish();
    }

    #[test]
    fn forwards_is_tag_enabled() {
        #[derive(Default)]
        struct CallCheck {
            is_tag_enabled_count: u32,
        }

        impl mock_atrace::ATraceMocker for CallCheck {
            fn atrace_is_tag_enabled_wrap(&mut self, tag: u64) -> u64 {
                self.is_tag_enabled_count += 1;
                assert!(self.is_tag_enabled_count < 2);
                assert_eq!(tag, cutils_trace_bindgen::ATRACE_TAG_ADB as u64);
                1
            }

            fn finish(&self) {
                assert_eq!(self.is_tag_enabled_count, 1);
            }
        }

        let _guard = mock_atrace::set_scoped_mocker(CallCheck::default());

        let res = atrace_is_tag_enabled(AtraceTag::Adb);
        assert!(res);

        mock_atrace::mocker_finish();
    }
}
