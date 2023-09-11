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

//! Tracing-subscriber layer for libatrace_rust.

use ::atrace::AtraceTag;
use tracing::{Event, Id, Subscriber};
use tracing_subscriber::layer::{Context, Layer};
use tracing_subscriber::registry::LookupSpan;

use tracing::span::Attributes;
use tracing::span::Record;
use tracing_subscriber::field::Visit;

/// Subscriber layer that forwards events to ATrace.
pub struct AtraceSubscriber {
    tag: AtraceTag,
    should_record_fields: bool,
}

impl Default for AtraceSubscriber {
    fn default() -> Self {
        Self::new(AtraceTag::App)
    }
}

impl AtraceSubscriber {
    /// Makes a new subscriber with tag.
    pub fn new(tag: AtraceTag) -> AtraceSubscriber {
        AtraceSubscriber { tag, should_record_fields: true }
    }

    /// Disables recording of field values.
    pub fn without_fields(self) -> AtraceSubscriber {
        AtraceSubscriber { tag: self.tag, should_record_fields: false }
    }
}

struct FieldFormatter {
    is_event: bool,
    s: String,
}

impl FieldFormatter {
    fn for_event() -> FieldFormatter {
        FieldFormatter { is_event: true, s: String::new() }
    }
    fn for_span() -> FieldFormatter {
        FieldFormatter { is_event: false, s: String::new() }
    }
}

impl FieldFormatter {
    fn as_string(&self) -> &String {
        &self.s
    }
    fn is_empty(&self) -> bool {
        self.s.is_empty()
    }
}

impl Visit for FieldFormatter {
    fn record_debug(&mut self, field: &tracing::field::Field, value: &dyn std::fmt::Debug) {
        if !self.s.is_empty() {
            self.s.push_str(", ");
        }
        if self.is_event && field.name() == "message" {
            self.s.push_str(&format!("{:?}", value));
        } else {
            self.s.push_str(&format!("{} = {:?}", field.name(), value));
        }
    }
}

impl<S: Subscriber + for<'lookup> LookupSpan<'lookup>> Layer<S> for AtraceSubscriber {
    fn on_new_span(&self, attrs: &Attributes, id: &Id, ctx: Context<S>) {
        if self.should_record_fields {
            let mut formatter = FieldFormatter::for_span();
            attrs.record(&mut formatter);
            ctx.span(id).unwrap().extensions_mut().insert(formatter)
        }
    }

    fn on_record(&self, span: &Id, values: &Record, ctx: Context<S>) {
        if self.should_record_fields {
            values.record(
                ctx.span(span).unwrap().extensions_mut().get_mut::<FieldFormatter>().unwrap(),
            );
        }
    }

    fn on_enter(&self, id: &Id, ctx: Context<S>) {
        let span = ctx.span(id).unwrap();
        let mut span_str = String::from(span.metadata().name());
        if self.should_record_fields {
            let span_extensions = span.extensions();
            let formatter = span_extensions.get::<FieldFormatter>().unwrap();
            if !formatter.is_empty() {
                span_str.push_str(", ");
                span_str.push_str(formatter.as_string());
            }
        }
        atrace::atrace_begin(self.tag, &span_str);
    }

    fn on_exit(&self, _id: &Id, _ctx: Context<S>) {
        atrace::atrace_end(self.tag);
    }

    fn on_event(&self, event: &Event, _ctx: Context<S>) {
        let mut event_str = String::new();
        if self.should_record_fields {
            let mut formatter = FieldFormatter::for_event();
            event.record(&mut formatter);
            event_str = formatter.as_string().clone();
        } else {
            struct MessageVisitor<'a> {
                s: &'a mut String,
            }
            impl Visit for MessageVisitor<'_> {
                fn record_debug(
                    &mut self,
                    field: &tracing::field::Field,
                    value: &dyn std::fmt::Debug,
                ) {
                    if field.name() == "message" {
                        self.s.push_str(&format!("{:?}", value));
                    }
                }
            }
            event.record(&mut MessageVisitor { s: &mut event_str });
        }
        if event_str.is_empty() {
            event_str = format!("{} event", event.metadata().level().as_str());
        }
        atrace::atrace_instant(self.tag, &event_str);
    }
}

#[cfg(test)]
use self::tests::mock_atrace as atrace;

#[cfg(test)]
mod tests {
    use super::*;
    use tracing::Level;
    use tracing_subscriber::prelude::__tracing_subscriber_SubscriberExt;

    pub mod mock_atrace {
        use atrace::AtraceTag;
        use std::cell::RefCell;

        /// Contains logic to check binding calls.
        /// Implement this trait in the test with mocking logic and checks in implemented functions.
        /// Default implementations panic.
        pub trait ATraceMocker {
            fn atrace_begin(&mut self, _tag: AtraceTag, _name: &str) {
                panic!("Unexpected call");
            }

            fn atrace_end(&mut self, _tag: AtraceTag) {
                panic!("Unexpected call");
            }

            fn atrace_instant(&mut self, _tag: AtraceTag, _name: &str) {
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
        /// with_mocker(|mocker| mocker.atrace_begin(tag, name))
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

        pub fn atrace_begin(tag: AtraceTag, name: &str) {
            with_mocker(|m| m.atrace_begin(tag, name))
        }

        pub fn atrace_end(tag: AtraceTag) {
            with_mocker(|m| m.atrace_end(tag))
        }

        pub fn atrace_instant(tag: AtraceTag, name: &str) {
            with_mocker(|m| m.atrace_instant(tag, name))
        }
    }

    #[test]
    fn emits_span_begin() {
        #[derive(Default)]
        struct CallCheck {
            begin_count: u32,
        }
        impl mock_atrace::ATraceMocker for CallCheck {
            fn atrace_begin(&mut self, tag: AtraceTag, name: &str) {
                self.begin_count += 1;
                assert!(self.begin_count < 2);
                assert_eq!(tag, AtraceTag::App);
                assert_eq!(name, "test span");
            }
            fn atrace_end(&mut self, _tag: AtraceTag) {}

            fn finish(&self) {
                assert_eq!(self.begin_count, 1);
            }
        }
        let _mock_guard = mock_atrace::set_scoped_mocker(CallCheck::default());

        let _subscriber_guard = tracing::subscriber::set_default(
            tracing_subscriber::registry().with(AtraceSubscriber::default()),
        );

        let _span = tracing::info_span!("test span").entered();

        mock_atrace::mocker_finish();
    }

    #[test]
    fn emits_span_end() {
        #[derive(Default)]
        struct CallCheck {
            end_count: u32,
        }
        impl mock_atrace::ATraceMocker for CallCheck {
            fn atrace_begin(&mut self, _tag: AtraceTag, _name: &str) {}
            fn atrace_end(&mut self, tag: AtraceTag) {
                self.end_count += 1;
                assert!(self.end_count < 2);
                assert_eq!(tag, AtraceTag::App);
            }

            fn finish(&self) {
                assert_eq!(self.end_count, 1);
            }
        }
        let _mock_guard = mock_atrace::set_scoped_mocker(CallCheck::default());

        let _subscriber_guard = tracing::subscriber::set_default(
            tracing_subscriber::registry().with(AtraceSubscriber::default()),
        );

        {
            let _span = tracing::info_span!("test span").entered();
        }

        mock_atrace::mocker_finish();
    }

    #[test]
    fn span_begin_end_is_ordered() {
        #[derive(Default)]
        struct CallCheck {
            begin_count: u32,
            instant_count: u32,
            end_count: u32,
        }
        impl mock_atrace::ATraceMocker for CallCheck {
            fn atrace_begin(&mut self, _tag: AtraceTag, _name: &str) {
                assert_eq!(self.end_count, 0);
                assert_eq!(self.instant_count, 0);

                self.begin_count += 1;
                assert!(self.begin_count < 2);
            }
            fn atrace_instant(&mut self, _tag: AtraceTag, _name: &str) {
                assert_eq!(self.begin_count, 1);
                assert_eq!(self.end_count, 0);

                self.instant_count += 1;
                assert!(self.instant_count < 2);
            }
            fn atrace_end(&mut self, _tag: AtraceTag) {
                assert_eq!(self.begin_count, 1);
                assert_eq!(self.instant_count, 1);

                self.end_count += 1;
                assert!(self.end_count < 2);
            }

            fn finish(&self) {
                assert_eq!(self.begin_count, 1);
                assert_eq!(self.end_count, 1);
                assert_eq!(self.instant_count, 1);
            }
        }
        let _mock_guard = mock_atrace::set_scoped_mocker(CallCheck::default());

        let _subscriber_guard = tracing::subscriber::set_default(
            tracing_subscriber::registry().with(AtraceSubscriber::default()),
        );

        {
            let _span = tracing::info_span!("span").entered();
            tracing::info!("test info");
        }

        mock_atrace::mocker_finish();
    }

    #[test]
    fn emits_instant_event() {
        #[derive(Default)]
        struct CallCheck {
            instant_count: u32,
        }
        impl mock_atrace::ATraceMocker for CallCheck {
            fn atrace_instant(&mut self, tag: AtraceTag, name: &str) {
                self.instant_count += 1;
                assert!(self.instant_count < 2);
                assert_eq!(tag, AtraceTag::App);
                assert_eq!(name, "test info");
            }

            fn finish(&self) {
                assert_eq!(self.instant_count, 1);
            }
        }
        let _mock_guard = mock_atrace::set_scoped_mocker(CallCheck::default());

        let _subscriber_guard = tracing::subscriber::set_default(
            tracing_subscriber::registry().with(AtraceSubscriber::default()),
        );

        tracing::info!("test info");

        mock_atrace::mocker_finish();
    }

    #[test]
    fn formats_event_without_message_with_fields_disabled() {
        #[derive(Default)]
        struct CallCheck {
            instant_count: u32,
        }
        impl mock_atrace::ATraceMocker for CallCheck {
            fn atrace_instant(&mut self, _tag: AtraceTag, name: &str) {
                self.instant_count += 1;
                assert!(self.instant_count < 2);
                assert_eq!(name, "DEBUG event");
            }

            fn finish(&self) {
                assert_eq!(self.instant_count, 1);
            }
        }
        let _mock_guard = mock_atrace::set_scoped_mocker(CallCheck::default());

        let _subscriber_guard = tracing::subscriber::set_default(
            tracing_subscriber::registry().with(AtraceSubscriber::default().without_fields()),
        );

        tracing::debug!(foo = 1);

        mock_atrace::mocker_finish();
    }

    #[test]
    fn formats_event_without_message_with_fields_enabled() {
        #[derive(Default)]
        struct CallCheck {
            instant_count: u32,
        }
        impl mock_atrace::ATraceMocker for CallCheck {
            fn atrace_instant(&mut self, _tag: AtraceTag, name: &str) {
                self.instant_count += 1;
                assert!(self.instant_count < 2);
                assert_eq!(name, "foo = 1");
            }

            fn finish(&self) {
                assert_eq!(self.instant_count, 1);
            }
        }
        let _mock_guard = mock_atrace::set_scoped_mocker(CallCheck::default());

        let _subscriber_guard = tracing::subscriber::set_default(
            tracing_subscriber::registry().with(AtraceSubscriber::default()),
        );

        tracing::debug!(foo = 1);

        mock_atrace::mocker_finish();
    }

    #[test]
    fn can_set_tag() {
        #[derive(Default)]
        struct CallCheck {
            begin_count: u32,
            instant_count: u32,
            end_count: u32,
        }
        impl mock_atrace::ATraceMocker for CallCheck {
            fn atrace_begin(&mut self, tag: AtraceTag, _name: &str) {
                self.begin_count += 1;
                assert!(self.begin_count < 2);
                assert_eq!(tag, AtraceTag::WindowManager);
            }
            fn atrace_instant(&mut self, tag: AtraceTag, _name: &str) {
                self.instant_count += 1;
                assert!(self.instant_count < 2);
                assert_eq!(tag, AtraceTag::WindowManager);
            }
            fn atrace_end(&mut self, tag: AtraceTag) {
                self.end_count += 1;
                assert!(self.end_count < 2);
                assert_eq!(tag, AtraceTag::WindowManager);
            }

            fn finish(&self) {
                assert_eq!(self.begin_count, 1);
                assert_eq!(self.end_count, 1);
                assert_eq!(self.instant_count, 1);
            }
        }
        let _mock_guard = mock_atrace::set_scoped_mocker(CallCheck::default());

        let _subscriber_guard = tracing::subscriber::set_default(
            tracing_subscriber::registry().with(AtraceSubscriber::new(AtraceTag::WindowManager)),
        );

        {
            let _span = tracing::info_span!("span").entered();
            tracing::info!("test info");
        }

        mock_atrace::mocker_finish();
    }

    #[test]
    fn fields_ignored_when_disabled() {
        #[derive(Default)]
        struct CallCheck {
            begin_count: u32,
            instant_count: u32,
        }
        impl mock_atrace::ATraceMocker for CallCheck {
            fn atrace_begin(&mut self, _tag: AtraceTag, name: &str) {
                self.begin_count += 1;
                assert!(self.begin_count < 2);
                assert_eq!(name, "test span");
            }
            fn atrace_instant(&mut self, _tag: AtraceTag, name: &str) {
                self.instant_count += 1;
                assert!(self.instant_count < 2);
                assert_eq!(name, "test info");
            }
            fn atrace_end(&mut self, _tag: AtraceTag) {}
            fn finish(&self) {
                assert_eq!(self.begin_count, 1);
                assert_eq!(self.instant_count, 1);
            }
        }
        let _mock_guard = mock_atrace::set_scoped_mocker(CallCheck::default());

        let _subscriber_guard = tracing::subscriber::set_default(
            tracing_subscriber::registry().with(AtraceSubscriber::default().without_fields()),
        );

        let _span = tracing::info_span!("test span", bar = "foo").entered();
        tracing::event!(Level::INFO, foo = "bar", "test info");

        mock_atrace::mocker_finish();
    }

    #[test]
    fn formats_instant_event_fields() {
        #[derive(Default)]
        struct CallCheck {
            instant_count: u32,
        }
        impl mock_atrace::ATraceMocker for CallCheck {
            fn atrace_instant(&mut self, _tag: AtraceTag, name: &str) {
                self.instant_count += 1;
                assert!(self.instant_count < 2);
                assert_eq!(name, "test info, foo = \"bar\", baz = 5");
            }
            fn finish(&self) {
                assert_eq!(self.instant_count, 1);
            }
        }
        let _mock_guard = mock_atrace::set_scoped_mocker(CallCheck::default());

        let _subscriber_guard = tracing::subscriber::set_default(
            tracing_subscriber::registry().with(AtraceSubscriber::default()),
        );

        tracing::event!(Level::INFO, foo = "bar", baz = 5, "test info");

        mock_atrace::mocker_finish();
    }

    #[test]
    fn formats_span_fields() {
        #[derive(Default)]
        struct CallCheck {
            begin_count: u32,
        }
        impl mock_atrace::ATraceMocker for CallCheck {
            fn atrace_begin(&mut self, _tag: AtraceTag, name: &str) {
                self.begin_count += 1;
                assert!(self.begin_count < 2);
                assert_eq!(name, "test span, foo = \"bar\", baz = 5");
            }
            fn atrace_end(&mut self, _tag: AtraceTag) {}
            fn finish(&self) {
                assert_eq!(self.begin_count, 1);
            }
        }
        let _mock_guard = mock_atrace::set_scoped_mocker(CallCheck::default());

        let _subscriber_guard = tracing::subscriber::set_default(
            tracing_subscriber::registry().with(AtraceSubscriber::default()),
        );

        let _span = tracing::info_span!("test span", foo = "bar", baz = 5).entered();

        mock_atrace::mocker_finish();
    }
}
