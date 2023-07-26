# libatrace_rust - ATrace bindings for Rust

Wrapper library for ATrace methods from libcutils.

## Quick start

Add the library to your `rustlibs` in `Android.bp`:

```text
rustlibs: [
    ...
    "libatrace_rust",
    ...
],
```

Call tracing methods:

```rust
fn important_function() {
    // Use this macro to trace a function.
    atrace::trace_method!(AtraceTag::App);

    if condition {
        // Use a scoped event to trace inside a scope.
        let _event = atrace::begin_scoped_event(AtraceTag::App, "Inside a scope");
        ...
    }

    // Or just use the wrapped API directly.
    atrace::atrace_begin(AtraceTag::App, "My event");
    ...
    atrace::atrace_end(AtraceTag::App)
}
```

See more in the [example](./example/src/main.rs).

You're all set! Now you can collect a trace with your favorite tracing tool like
[Perfetto](https://perfetto.dev/docs/data-sources/atrace).

## Performance

When tracing is enabled, you can expect 1-10 us per event - this is a significant cost that may
affect the performance of hot high-frequency methods. When the events are disabled, calling them is
cheap - on the order of 5-10 ns.

There is 10-20% overhead from the wrapper, mostly caused by string conversion when tracing is
enabled. You can find the numbers in [benchmark/README.md](./benchmark/README.md).
