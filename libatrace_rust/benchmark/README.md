# libatrace_rust benchmarks

Benchmarks to compare the performance of Rust ATrace bindings with directly calling the
`libcutils` methods from C++.

There are two binaries implementing the same benchmarks:

* `libatrace_rust_benchmark` (`atrace_benchmark.rs`) for Rust.
* `libatrace_rust_benchmark_cc` (`atrace_benchmark.cc`) for C++.

The benchmarks emit ATrace events with tracing off and tracing on. `atrace_begin` is measured
with short and long event names to check if the string length affects timings. For example,
`tracing_on_begin/1000` measures `atrace_begin` with a 1000-character name and tracing enabled.

## Running the benchmarks

To run the benchmarks, push the binaries to the device with `adb` and launch them via `adb shell`.
You may need to push dynamic libraries they depend on as well if they're not present on device and
run with `LD_LIBRARY_PATH`.

Do not enable ATrace collectors. The benchmarks effectively emit events in a loop and will spam
any trace and distort performance results.

The benchmarks will override system properties to enable or disable events, specifically ATrace App
event collection in `debug.atrace.app_number` and `debug.atrace.app_0`. After a successful execution
the events will be disabled.

## Results

The timings are not representative of actual cost of fully enabling tracing, only of emitting
events via API, since there's nothing receiving the events.

The tests were done on a `aosp_cf_x86_64_phone-userdebug` Cuttlefish VM. Execution times on real
device may be different but we expect similar relative performance between Rust wrappers and C.

Rust results from `libatrace_rust_benchmark 2>&1 | grep time`:

```text
tracing_off_begin/10    time:   [91.195 ns 91.648 ns 92.193 ns]
tracing_off_begin/1000  time:   [289.50 ns 290.57 ns 291.76 ns]
tracing_off_end         time:   [6.0455 ns 6.0998 ns 6.1644 ns]
tracing_on_begin/10     time:   [1.2285 µs 1.2330 µs 1.2379 µs]
tracing_on_begin/1000   time:   [1.4667 µs 1.4709 µs 1.4754 µs]
tracing_on_end          time:   [1.1344 µs 1.1444 µs 1.1543 µs]
```

C++ results from `libatrace_rust_benchmark_cc`:

```text
------------------------------------------------------------------------
Benchmark                              Time             CPU   Iterations
------------------------------------------------------------------------
BM_TracingOffAtraceBegin/10         4.00 ns         3.96 ns    175953732
BM_TracingOffAtraceBegin/1000       4.05 ns         4.02 ns    176298494
BM_TracingOffAtraceEnd              4.08 ns         4.05 ns    176422059
BM_TracingOnAtraceBegin/10          1119 ns         1110 ns       640816
BM_TracingOnAtraceBegin/1000        1151 ns         1142 ns       615781
BM_TracingOnAtraceEnd               1076 ns         1069 ns       653646
```

*If you notice that measurements with tracing off and tracing on have similar times, it might mean
that enabling ATrace events failed and you need to debug the benchmark.*
