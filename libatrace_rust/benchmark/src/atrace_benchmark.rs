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

//! Benchmark for ATrace bindings.

use atrace::AtraceTag;
use criterion::{BenchmarkId, Criterion};

extern "C" {
    fn disable_app_atrace();
    fn enable_atrace_for_single_app(name: *const std::os::raw::c_char);
}

fn turn_tracing_off() {
    unsafe {
        disable_app_atrace();
    }
}

fn turn_tracing_on() {
    let procname = std::ffi::CString::new(std::env::args().next().unwrap()).unwrap();
    unsafe {
        enable_atrace_for_single_app(procname.as_ptr());
    }
}

fn new_criterion() -> Criterion {
    let path = "/data/local/tmp/criterion/benchmarks";
    std::fs::create_dir_all(path).unwrap_or_else(|e| {
        panic!("The criterion folder should be possible to create at {}: {}", path, e)
    });
    std::env::set_var("CRITERION_HOME", path);
    Criterion::default()
}

fn bench_tracing_off_begin(c: &mut Criterion, name_len: usize) {
    turn_tracing_off();
    let name = "0".repeat(name_len);
    c.bench_with_input(BenchmarkId::new("tracing_off_begin", name_len), &name, |b, name| {
        b.iter(|| atrace::atrace_begin(AtraceTag::App, name.as_str()))
    });
}

fn bench_tracing_off_end(c: &mut Criterion) {
    turn_tracing_off();
    c.bench_function("tracing_off_end", |b| b.iter(|| atrace::atrace_end(AtraceTag::App)));
}

fn bench_tracing_on_begin(c: &mut Criterion, name_len: usize) {
    turn_tracing_on();
    let name = "0".repeat(name_len);
    c.bench_with_input(BenchmarkId::new("tracing_on_begin", name_len), &name, |b, name| {
        b.iter(|| atrace::atrace_begin(AtraceTag::App, name.as_str()))
    });
    turn_tracing_off();
}

fn bench_tracing_on_end(c: &mut Criterion) {
    turn_tracing_on();
    c.bench_function("tracing_on_end", |b| b.iter(|| atrace::atrace_end(AtraceTag::App)));
    turn_tracing_off();
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut criterion = new_criterion();

    bench_tracing_off_begin(&mut criterion, 10);
    bench_tracing_off_begin(&mut criterion, 1000);
    bench_tracing_off_end(&mut criterion);

    bench_tracing_on_begin(&mut criterion, 10);
    bench_tracing_on_begin(&mut criterion, 1000);
    bench_tracing_on_end(&mut criterion);

    Ok(())
}
