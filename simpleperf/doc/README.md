# Simpleperf

Simpleperf is a native profiling tool for Android. It can be used to profile
both Android applications and native processes running on Android. It can
profile both Java and C++ code on Android. It can be used on Android L
and above.

Simpleperf is part of the Android Open Source Project. The source code is [here](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/).
The latest document is [here](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/doc/README.md).
Bugs and feature requests can be submitted at http://github.com/android-ndk/ndk/issues.


## Table of Contents

- [Introduction](#introduction)
- [Tools in simpleperf](#tools-in-simpleperf)
- [Android application profiling](#android-application-profiling)
    - [Prepare an Android application](#prepare-an-android-application)
    - [Record and report profiling data (using command-lines)](#record-and-report-profiling-data-using-commandlines)
    - [Record and report profiling data (using python scripts)](#record-and-report-profiling-data-using-python-scripts)
    - [Record and report call graphs](#record-and-report-call-graphs)
    - [Visualize profiling data](#visualize-profiling-data)
    - [Annotate source code](#annotate-source-code)
    - [Trace offcpu time](#trace-offcpu-time)
    - [Profile from launch of an application](#profile-from-launch-of-an-application)
- [Executable commands reference](#executable-commands-reference)
    - [How does simpleperf work?](#how-does-simpleperf-work)
    - [Commands](#commands)
    - [The list command](#the-list-command)
    - [The stat command](#the-stat-command)
        - [Select events to stat](#select-events-to-stat)
        - [Select target to stat](#select-target-to-stat)
        - [Decide how long to stat](#decide-how-long-to-stat)
        - [Decide the print interval](#decide-the-print-interval)
        - [Display counters in systrace](#display-counters-in-systrace)
    - [The record command](#the-record-command)
        - [Select events to record](#select-events-to-record)
        - [Select target to record](#select-target-to-record)
        - [Set the frequency to record](#set-the-frequency-to-record)
        - [Decide how long to record](#decide-how-long-to-record)
        - [Set the path to store profiling data](#set-the-path-to-store-profiling-data)
        - [Record call graphs](#record-call-graphs-in-record-cmd)
        - [Record both on CPU time and off CPU time](#record-both-on-cpu-time-and-off-cpu-time)
    - [The report command](#the-report-command)
        - [Set the path to read profiling data](#set-the-path-to-read-profiling-data)
        - [Set the path to find binaries](#set-the-path-to-find-binaries)
        - [Filter samples](#filter-samples)
        - [Group samples into sample entries](#group-samples-into-sample-entries)
        - [Report call graphs](#report-call-graphs-in-report-cmd)
- [Scripts reference](#scripts-reference)
    - [app_profiler py](#app_profiler-py)
        - [Profile from launch of an application](#profile-from-launch-of-an-application)
    - [binary_cache_builder.py](#binary_cache_builder-py)
    - [run_simpleperf_on_device.py](#run_simpleperf_on_device-py)
    - [report.py](#report-py)
    - [report_html.py](#report_html-py)
    - [inferno](#inferno)
    - [pprof_proto_generator.py](#pprof_proto_generator-py)
    - [report_sample.py](#report_sample-py)
    - [simpleperf_report_lib.py](#simpleperf_report_lib-py)
- [Answers to common issues](#answers-to-common-issues)
    - [Why we suggest profiling on android >= N devices](#why-we-suggest-profiling-on-android-n-devices)
    - [Suggestions about recording call graphs](#suggestions-about-recording-call-graphs)

## Introduction

Simpleperf contains two parts: the simpleperf executable and Python scripts.

The simpleperf executable works similar to linux-tools-perf, but has some specific features for
the Android profiling environment:

1. It collects more info in profiling data. Since the common workflow is "record on the device, and
   report on the host", simpleperf not only collects samples in profiling data, but also collects
   needed symbols, device info and recording time.

2. It delivers new features for recording.
   a. When recording dwarf based call graph, simpleperf unwinds the stack before writing a sample
      to file. This is to save storage space on the device.
   b. Support tracing both on CPU time and off CPU time with --trace-offcpu option.

3. It relates closely to the Android platform.
   a. Is aware of Android environment, like using system properties to enable profiling, using
      run-as to profile in application's context.
   b. Supports reading symbols and debug information from the .gnu_debugdata section, because
      system libraries are built with .gnu_debugdata section starting from Android O.
   c. Supports profiling shared libraries embedded in apk files.
   d. It uses the standard Android stack unwinder, so its results are consistent with all other
      Android tools.

4. It builds executables and shared libraries for different usages.
   a. Builds static executables on the device. Since static executables don't rely on any library,
      simpleperf executables can be pushed on any Android device and used to record profiling data.
   b. Builds executables on different hosts: Linux, Mac and Windows. These executables can be used
      to report on hosts.
   c. Builds report shared libraries on different hosts. The report library is used by different
      Python scripts to parse profiling data.

Detailed documentation for the simpleperf executable is [here](#executable-commands-reference).

Python scripts are split into three parts according to their functions:

1. Scripts used for simplifying recording, like app_profiler.py.

2. Scripts used for reporting, like report.py, report_html.py, inferno.

3. Scripts used for parsing profiling data, like simpleperf_report_lib.py.

Detailed documentation for the Python scripts is [here](#scripts-reference).

## Tools in simpleperf

The simpleperf executables and Python scripts are located in simpleperf/ in ndk releases, and in
system/extras/simpleperf/scripts/ in AOSP. Their functions are listed below.

bin/: contains executables and shared libraries.

bin/android/${arch}/simpleperf: static simpleperf executables used on the device.

bin/${host}/${arch}/simpleperf: simpleperf executables used on the host, only supports reporting.

bin/${host}/${arch}/libsimpleperf_report.${so/dylib/dll}: report shared libraries used on the host.

[app_profiler.py](#app_profiler-py): recording profiling data.

[binary_cache_builder.py](#binary_cache_builder-py): building binary cache for profiling data.

[report.py](#report-py): reporting in stdio interface.

[report_html.py](#report_html-py): reporting in html interface.

[inferno.sh](#inferno) (or inferno.bat on Windows): generating flamegraph in html interface.

inferno/: implementation of inferno. Used by inferno.sh.

[pprof_proto_generator.py](#pprof_proto_generator-py): converting profiling data to the format
       used by [pprof](https://github.com/google/pprof).

[report_sample.py](#report_sample-py): converting profiling data to the format used by [FlameGraph](https://github.com/brendangregg/FlameGraph).

[simpleperf_report_lib.py](#simpleperf_report_lib-py): library for parsing profiling data.

## Android application profiling

This section shows how to profile an Android application.
[Here](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/demo/README.md) are examples. And we use
[SimpleperfExamplePureJava](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/demo/SimpleperfExamplePureJava) project to show the profiling results.

Simpleperf only supports profiling native instructions in binaries in ELF
format. If the Java code is executed by interpreter, or with jit cache, it
can’t be profiled by simpleperf. As Android supports Ahead-of-time compilation,
it can compile Java bytecode into native instructions with debug information.
On devices with Android version <= M, we need root privilege to compile Java
bytecode with debug information. However, on devices with Android version >= N,
we don't need root privilege to do so.

Profiling an Android application involves three steps:
1. Prepare the application.
2. Record profiling data.
3. Report profiling data.

To profile, we can use either command lines or python scripts. Below shows both.


### Prepare an Android application

Before profiling, we need to install the application to be profiled on an Android device.
To get valid profiling results, please check following points:

**1. The application should be debuggable.**
It means [android:debuggable](https://developer.android.com/guide/topics/manifest/application-element.html#debug)
should be true. So we need to use debug [build type](https://developer.android.com/studio/build/build-variants.html#build-types)
instead of release build type. It is understandable because we can't profile others' apps.
However, on a rooted Android device, the application doesn't need to be debuggable.

**2. Run on an Android >= N device.**
We suggest profiling on an Android >= N device. The reason is [here](#why-we-suggest-profiling-on-android-n-devices).


**3. On Android O, add `wrap.sh` in the apk.**
To profile Java code, we need ART running in oat mode. But on Android O,
debuggable applications are forced to run in jit mode. To work around this,
we need to add a `wrap.sh` in the apk. So if you are running on Android O device,
Check [here](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/demo/SimpleperfExamplePureJava/app/profiling.gradle)
for how to add `wrap.sh` in the apk.

**4. Make sure C++ code is compiled with optimizing flags.**
If the application contains C++ code, it can be compiled with -O0 flag in debug build type.
This makes C++ code slow. Check [here](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/demo/SimpleperfExamplePureJava/app/profiling.gradle)
for how to avoid that.

**5. Use native libraries with debug info in the apk when possible.**
If the application contains C++ code or pre-compiled native libraries, try to use
unstripped libraries in the apk. This helps simpleperf generating better profiling
results. Check [here](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/demo/SimpleperfExamplePureJava/app/profiling.gradle)
for how to use unstripped libraries.

Here we use [SimpleperfExamplePureJava](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/demo/SimpleperfExamplePureJava) as an example.
It builds an app-profiling.apk for profiling.

    $ git clone https://android.googlesource.com/platform/system/extras
    $ cd extras/simpleperf/demo
    # Open SimpleperfExamplesPureJava project with Android studio,
    # and build this project sucessfully, otherwise the `./gradlew` command below will fail.
    $ cd SimpleperfExamplePureJava

    # On windows, use "gradlew" instead.
    $ ./gradlew clean assemble
    $ adb install -r app/build/outputs/apk/app-profiling.apk


### Record and report profiling data (using command-lines)

We recommend using Python scripts for profiling because they are more convenient.
But using command-line will give us a better understanding of the profile process
step by step. So we first show how to use command lines.

**1. Enable profiling**

    $ adb shell setprop security.perf_harden 0

**2. Fully compile the app**

We need to compile Java bytecode into native instructions to profile Java code
in the application. This needs different commands on different Android versions.

On Android >= N:

    $ adb shell setprop debug.generate-debug-info true
    $ adb shell cmd package compile -f -m speed com.example.simpleperf.simpleperfexamplepurejava
    # Restart the app to take effect
    $ adb shell am force-stop com.example.simpleperf.simpleperfexamplepurejava

On Android M devices, we need root privilege to force Android to fully compile
Java code into native instructions in ELF binaries with debug information. We
also need root privilege to read compiled native binaries (because installd
writes them to a directory whose uid/gid is system:install). So profiling Java
code can only be done on rooted devices.

    $ adb root
    $ adb shell setprop dalvik.vm.dex2oat-flags -g

    # Reinstall the app.
    $ adb install -r app/build/outputs/apk/app-profiling.apk

On Android L devices, we also need root privilege to compile the app with debug info
and access the native binaries.

    $ adb root
    $ adb shell setprop dalvik.vm.dex2oat-flags --include-debug-symbols

    # Reinstall the app.
    $ adb install -r app/build/outputs/apk/app-profiling.apk


**3. Start the app if needed**

    $ adb shell am start -n com.example.simpleperf.simpleperfexamplepurejava/.MainActivity

    $ adb shell pidof com.example.simpleperf.simpleperfexamplepurejava
    6885

So the id of the app process is `6885`. We will use this number in the command lines below,
please replace this number with what you get by running `pidof` command.
On Android <= M, pidof may not exist or work well, and you can try
`ps | grep com.example.simpleperf.simpleperfexamplepurejava` instead.

**4. Download simpleperf to the app's data directory**

    # Find which architecture the app is using. On ARM devices, it must be ARM.
    # But on ARM64 devices, it can be either ARM or ARM64. If you are not sure,
    # you can find it out in the app process's map.
    $ adb shell pidof com.example.simpleperf.simpleperfexamplepurejava
    6885
    $ adb shell run-as com.example.simpleperf.simpleperfexamplepurejava cat /proc/6885/maps | grep boot.oat
    708e6000-70e33000 r--p 00000000 103:09 1214                              /system/framework/arm64/boot.oat

    # The app uses /arm64/boot.oat, so push simpleperf in bin/android/arm64/ to device.

    # Now download the simpleperf for the app's architecture on the device.
    $ cd ../../scripts/
    $ adb push bin/android/arm64/simpleperf /data/local/tmp
    $ adb shell chmod a+x /data/local/tmp/simpleperf


**5. Record perf.data**

    $ adb shell /data/local/tmp/simpleperf record \
      --app com.example.simpleperf.simpleperfexamplepurejava --duration 10 \
      -o /data/local/tmp/perf.data
    simpleperf I 04-27 20:41:11  6940  6940 cmd_record.cpp:357] Samples recorded: 40008. Samples lost: 0.

The profiling data is recorded at /data/local/tmp/perf.data.

Normally we need to use the app when profiling, otherwise we may record no samples.
But in this case, the MainActivity starts a busy thread. So we don't need to use
the app while profiling.

There are many options to record profiling data, check [record command](#simpleperf-record) for details.

**6. Report perf.data**

    # Pull perf.data on the host.
    $ adb pull /data/local/tmp/perf.data

    # Report samples using report.py, report.py is a python wrapper of simpleperf report command.
    $ python report.py
    ...
    Overhead  Command   Pid   Tid   Shared Object                                                                     Symbol
    83.54%    Thread-2  6885  6900  /data/app/com.example.simpleperf.simpleperfexamplepurejava-2/oat/arm64/base.odex  void com.example.simpleperf.simpleperfexamplepurejava.MainActivity$1.run()
    16.11%    Thread-2  6885  6900  /data/app/com.example.simpleperf.simpleperfexamplepurejava-2/oat/arm64/base.odex  int com.example.simpleperf.simpleperfexamplepurejava.MainActivity$1.callFunction(int)

There are many ways to show reports, check [report command](#simpleperf-report) for details.


### Record and report profiling data (using python scripts)

Besides command lines, we can use `app-profiler.py` to profile Android applications.
It downloads simpleperf on the device, records perf.data, and collects profiling
results and native binaries on the host.

**1. Record perf.data by running `app-profiler.py`**

    $ python app_profiler.py --app com.example.simpleperf.simpleperfexamplepurejava \
         --apk ../SimpleperfExamplePureJava/app/build/outputs/apk/app-profiling.apk \
         -r "-e cpu-cycles:u --duration 10"


If running successfully, it will collect profiling data in perf.data in current
directory, and related native binaries in binary_cache/.

**2. Report perf.data**

We can use `report.py` to report perf.data.

    $ python report.py

We can add any option accepted by `simpleperf report` command to `report.py`.


### Record and report call graphs

A call graph is a tree showing function call relations. Below is an example.

    main() {
        FunctionOne();
        FunctionTwo();
    }
    FunctionOne() {
        FunctionTwo();
        FunctionThree();
    }
    a call graph:
        main-> FunctionOne
           |    |
           |    |-> FunctionTwo
           |    |-> FunctionThree
           |
           |-> FunctionTwo


#### Record dwarf based call graphs

When using command lines, add -g as below:

    $ adb shell /data/local/tmp/simpleperf record -g \
    --app com.example.simpleperf.simpleperfexamplepurejava --duration 10 \
    -o /data/local/tmp/perf.data

When using app_profiler.py, add -g as below:

    $ python app_profiler.py --app com.example.simpleperf.simpleperfexamplepurejava \
        --apk ../SimpleperfExamplePureJava/app/build/outputs/apk/app-profiling.apk \
        -r "-e cpu-cycles:u --duration 10 -g"

Recording dwarf based call graphs needs support of debug information
in native binaries. So if using native libraries in the application,
it is better to contain non-stripped native libraries in the apk.


#### Record stack frame based call graphs

When using command lines, add "--call-graph fp" as below:

    $ adb shell /data/local/tmp/simpleperf record --call-graph fp \
    --app com.example.simpleperf.simpleperfexamplepurejava --duration 10 \
    -o /data/local/tmp/perf.data

When using app_profiler.py, add "--call-graph fp" as below:

    $ python app_profiler.py --app com.example.simpleperf.simpleperfexamplepurejava \
        --apk ../SimpleperfExamplePureJava/app/build/outputs/apk/app-profiling.apk \
        -r "-e cpu-cycles:u --duration 10 --call-graph fp"

Recording stack frame based call graphs needs support of stack frame
registers. Notice that on ARM, stack frame registers
are not supported well, even if compiled using "-O0 -g -fno-omit-frame-pointer"
It is because the kernel can't unwind user stack containing both
ARM/THUMB code. **So please consider recording dwarf based call graphs on ARM, or profiling
on ARM64.**


#### Report call graphs

To report call graphs using command lines, add -g.

    $ python report.py -g
    ...
    Children  Self    Command          Pid    Tid    Shared Object                                                                     Symbol
    99.97%    0.00%   Thread-2         10859  10876  /system/framework/arm64/boot.oat                                                  java.lang.Thread.run
       |
       -- java.lang.Thread.run
          |
           -- void com.example.simpleperf.simpleperfexamplepurejava.MainActivity$1.run()
               |--83.66%-- [hit in function]
               |
               |--16.22%-- int com.example.simpleperf.simpleperfexamplepurejava.MainActivity$1.callFunction(int)
               |    |--99.97%-- [hit in function]

To report call graphs in gui mode, add --gui.

    $ python report.py -g --gui
    # Double-click an item started with '+' to show its call graph.

### Visualize profiling data

`simpleperf_report_lib.py` provides an interface reading samples from perf.data.
By using it, You can write python scripts to read perf.data or convert perf.data
to other formats. Below are two examples.


### Show flamegraph

After collecting perf.data, you can use [inferno](./inferno.md) to show
flamegraphs.
On non-Windows platforms:

    $ ./inferno.sh -sc --symfs binary_cache

On Windows platform:

    $ inferno.bat -sc --symfs binary_cache

Remove `--symfs binary_cache` if you selected not to collect binaries when
using `app_profiler.py`.

You can also build flamegraphs based on scripts in
https://github.com/brendangregg/FlameGraph. Please make sure you have perl
installed.

    $ git clone https://github.com/brendangregg/FlameGraph.git
    $ python report_sample.py --symfs binary_cache >out.perf
    $ FlameGraph/stackcollapse-perf.pl out.perf >out.folded
    $ FlameGraph/flamegraph.pl out.folded >a.svg


### Visualize using pprof

pprof is a tool for visualization and analysis of profiling data. It can
be got from https://github.com/google/pprof. pprof_proto_generator.py can
generate profiling data in a format acceptable by pprof.

    $ python pprof_proto_generator.py
    $ pprof -pdf pprof.profile


### Annotate source code

`annotate.py` reads perf.data, binaries in `binary-cache` (collected by `app-profiler.py`)
and source code, and generates annoated source code in `annotated_files/`.

**1. Run annotate.py**

    $ python annotate.py -s ../SimpleperfExamplePureJava

`addr2line` is need to annotate source code. It can be found in Android ndk
release, in paths like toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-addr2line.
Please use --addr2line to set the path of `addr2line` if annotate.py
can't find it.

**2. Read annotated code**

The annotated source code is located at `annotated_files/`.
`annotated_files/summary` shows how each source file is annotated.

One annotated source file is `annotated_files/java/com/example/simpleperf/simpleperfexamplepurejava/MainActivity.java`.
It's content is similar to below:

    // [file] shows how much time is spent in current file.
    /* [file] acc_p: 99.966552%, p: 99.837438% */package com.example.simpleperf.simpleperfexamplepurejava;
    ...
    // [func] shows how much time is spent in current function.
    /* [func] acc_p: 16.213395%, p: 16.209250% */            private int callFunction(int a) {
    ...
    // This shows how much time is spent in current line.
    // acc_p field means how much time is spent in current line and functions called by current line.
    // p field means how much time is spent just in current line.
    /* acc_p: 99.966552%, p: 83.628188%        */                    i = callFunction(i);


### Trace offcpu time

Simpleperf is a cpu profiler, it generates samples for a thread only when it is
running on a cpu. However, sometimes we want to find out where time of a thread
is spent, whether it is running on cpu, preempted by other threads, doing I/O
work, or waiting for some events. To support this, we added the --trace-offcpu
option in the simpleperf record command. When --trace-offcpu is used, simpleperf
generates a sample when a running thread is scheduled out, so we know the
callstack of a thread when it is scheduled out. And when reporting a perf.data
generated with --trace-offcpu, we use timestamp to the next sample
(instead of event counts from the previous sample) as the weight of the current
sample. As a result, we can get a call graph based on timestamp, including both
on cpu time and off cpu time.

trace-offcpu is implemented using sched:sched_switch tracepoint event, which
may not work well on old kernels. But it is guaranteed to be supported on
devices after Android O MR1. We can check whether trace-offcpu is supported as
below.

    $ python run_simpleperf_on_device.py list --show-features
    dwarf-based-call-graph
    trace-offcpu

If trace-offcpu is supported, it will be shown in the feature list.
Then we can try it. Below is an example without using --trace-offcpu.

    $ python app_profiler.py -p com.example.simpleperf.simpleperfexamplepurejava \
      -a .SleepActivity -r "-g -e cpu-cycles:u --duration 10"
    $ ./inferno.sh -sc

![flamegraph sample](./without_trace_offcpu.png)

In the graph, all time is taken by RunFunction(), and sleep time is ignored.
But if we add --trace-offcpu, the graph is changed as below.

    $ python app_profiler.py -p com.example.simpleperf.simpleperfexamplepurejava \
      -a .SleepActivity -r "-g -e cpu-cycles:u --trace-offcpu --duration 10"
    $ ./inferno.sh -sc

![flamegraph sample](./trace_offcpu.png)

As shown in the graph, half time is spent in RunFunction(), and half time is
spent in SleepFunction(). It includes both on cpu time and off cpu time.

### Profile from launch of an application

Sometimes we want to profile the launch-time of an application. To support this,
we added the --app option in the simpleperf record command. The --app option
sets the package name of the Android application to profile. If the app is not
already running, the simpleperf record command will poll for the app process in
a loop with an interval of 1ms. So to profile from launch of an application,
we can first start simpleperf record with --app, then start the app.
Below is an example.

    $ adb shell /data/local/tmp/simpleperf record -g \
    --app com.example.simpleperf.simpleperfexamplepurejava --duration 1 \
    -o /data/local/tmp/perf.data
    # Start the app manually or using the `am` command.

To make it convenient to use, app_profiler.py combines these in the
--profile_from_launch option. Below is an example.

    $ python app_profiler.py -p com.example.simpleperf.simpleperfexamplepurejava \
      -a .MainActivity --arch arm64 -r "-g -e cpu-cycles:u --duration 1" \
      --profile_from_launch

## Executable commands reference

### How does simpleperf work?

Modern CPUs have a hardware component called the performance monitoring unit (PMU). The PMU has
several hardware counters, counting events like how many cpu cycles have happened, how many
instructions have executed, or how many cache misses have happened.

The Linux kernel wraps these hardware counters into hardware perf events. In addition, the Linux
kernel also provides hardware independent software events and tracepoint events. The Linux kernel
exposes all events to userspace via the perf_event_open system call, which is used by simpleperf.

Simpleperf has three main commands: stat, record and report.

The stat command gives a summary of how many events have happened in the profiled processes in a
time period. Here’s how it works:
1. Given user options, simpleperf enables profiling by making a system call to the kernel.
2. The kernel enables counters while the profiled processes are running.
3. After profiling, simpleperf reads counters from the kernel, and reports a counter summary.

The record command records samples of the profiled processes in a time period. Here’s how it works:
1. Given user options, simpleperf enables profiling by making a system call to the kernel.
2. Simpleperf creates mapped buffers between simpleperf and the kernel.
3. The kernel enables counters while the profiled processes are running.
4. Each time a given number of events happen, the kernel dumps a sample to the mapped buffers.
5. Simpleperf reads samples from the mapped buffers and stores profiling data in a file called
   perf.data.

The report command reads perf.data and any shared libraries used by the profiled processes,
and outputs a report showing where the time was spent.

### Commands

Simpleperf supports several commands, listed below:

```
The dump command: dumps content in perf.data, used for debugging simpleperf.
The help command: prints help information for other commands.
The kmem command: collects kernel memory allocation information (will be replaced by Python scripts).
The list command: lists all event types supported on the Android device.
The record command: profiles processes and stores profiling data in perf.data.
The report command: reports profiling data in perf.data.
The report-sample command: reports each sample in perf.data, used for supporting integration of
                           simpleperf in Android Studio.
The stat command: profiles processes and prints counter summary.
```

Each command supports different options, which can be seen through help message.

```sh
# List all commands.
$ simpleperf --help

# Print help message for record command.
$ simpleperf record --help
```

Below describes the most frequently used commands, which are list, stat, record and report.

### The list command

The list command lists all events available on the device. Different devices may support different
events because they have different hardware and kernels.

```sh
$ simpleperf list
List of hw-cache events:
  branch-loads
  ...
List of hardware events:
  cpu-cycles
  instructions
  ...
List of software events:
  cpu-clock
  task-clock
  ...
```

On ARM/ARM64, the list command also shows a list of raw events, they are the events supported by
the ARM PMU on the device. The kernel has wrapped part of them into hardware events and hw-cache
events. For example, raw-cpu-cycles is wrapped into cpu-cycles, raw-instruction-retired is wrapped
into instructions. The raw events are provided in case we want to use some events supported on the
device, but unfortunately not wrapped by the kernel.

### The stat command

The stat command is used to get event counter values of the profiled processes. By passing options,
we can select which events to use, which processes/threads to monitor, how long to monitor and the
print interval.

```sh
# Stat using default events (cpu-cycles,instructions,...), and monitor process 7394 for 10 seconds.
$ simpleperf stat -p 7394 --duration 10
Performance counter statistics:

 1,320,496,145  cpu-cycles         # 0.131736 GHz                     (100%)
   510,426,028  instructions       # 2.587047 cycles per instruction  (100%)
     4,692,338  branch-misses      # 468.118 K/sec                    (100%)
886.008130(ms)  task-clock         # 0.088390 cpus used               (100%)
           753  context-switches   # 75.121 /sec                      (100%)
           870  page-faults        # 86.793 /sec                      (100%)

Total test time: 10.023829 seconds.
```

#### Select events to stat

We can select which events to use via -e.

```sh
# Stat event cpu-cycles.
$ simpleperf stat -e cpu-cycles -p 11904 --duration 10

# Stat event cache-references and cache-misses.
$ simpleperf stat -e cache-references,cache-misses -p 11904 --duration 10
```

When running the stat command, if the number of hardware events is larger than the number of
hardware counters available in the PMU, the kernel shares hardware counters between events, so each
event is only monitored for part of the total time. In the example below, there is a percentage at
the end of each row, showing the percentage of the total time that each event was actually
monitored.

```sh
# Stat using event cache-references, cache-references:u,....
$ simpleperf stat -p 7394 -e cache-references,cache-references:u,cache-references:k \
      -e cache-misses,cache-misses:u,cache-misses:k,instructions --duration 1
Performance counter statistics:

4,331,018  cache-references     # 4.861 M/sec    (87%)
3,064,089  cache-references:u   # 3.439 M/sec    (87%)
1,364,959  cache-references:k   # 1.532 M/sec    (87%)
   91,721  cache-misses         # 102.918 K/sec  (87%)
   45,735  cache-misses:u       # 51.327 K/sec   (87%)
   38,447  cache-misses:k       # 43.131 K/sec   (87%)
9,688,515  instructions         # 10.561 M/sec   (89%)

Total test time: 1.026802 seconds.
```

In the example above, each event is monitored about 87% of the total time. But there is no
guarantee that any pair of events are always monitored at the same time. If we want to have some
events monitored at the same time, we can use --group.

```sh
# Stat using event cache-references, cache-references:u,....
$ simpleperf stat -p 7964 --group cache-references,cache-misses \
      --group cache-references:u,cache-misses:u --group cache-references:k,cache-misses:k \
      -e instructions --duration 1
Performance counter statistics:

3,638,900  cache-references     # 4.786 M/sec          (74%)
   65,171  cache-misses         # 1.790953% miss rate  (74%)
2,390,433  cache-references:u   # 3.153 M/sec          (74%)
   32,280  cache-misses:u       # 1.350383% miss rate  (74%)
  879,035  cache-references:k   # 1.251 M/sec          (68%)
   30,303  cache-misses:k       # 3.447303% miss rate  (68%)
8,921,161  instructions         # 10.070 M/sec         (86%)

Total test time: 1.029843 seconds.
```

#### Select target to stat

We can select which processes or threads to monitor via -p or -t. Monitoring a
process is the same as monitoring all threads in the process. Simpleperf can also fork a child
process to run the new command and then monitor the child process.

```sh
# Stat process 11904 and 11905.
$ simpleperf stat -p 11904,11905 --duration 10

# Stat thread 11904 and 11905.
$ simpleperf stat -t 11904,11905 --duration 10

# Start a child process running `ls`, and stat it.
$ simpleperf stat ls

# Stat a debuggable Android application.
$ simpleperf stat --app com.example.simpleperf.simpleperfexamplewithnative

# Stat system wide using -a.
$ simpleperf stat -a --duration 10
```

#### Decide how long to stat

When monitoring existing threads, we can use --duration to decide how long to monitor. When
monitoring a child process running a new command, simpleperf monitors until the child process ends.
In this case, we can use Ctrl-C to stop monitoring at any time.

```sh
# Stat process 11904 for 10 seconds.
$ simpleperf stat -p 11904 --duration 10

# Stat until the child process running `ls` finishes.
$ simpleperf stat ls

# Stop monitoring using Ctrl-C.
$ simpleperf stat -p 11904 --duration 10
^C
```

If you want to write a script to control how long to monitor, you can send one of SIGINT, SIGTERM,
SIGHUP signals to simpleperf to stop monitoring.

#### Decide the print interval

When monitoring perf counters, we can also use --interval to decide the print interval.

```sh
# Print stat for process 11904 every 300ms.
$ simpleperf stat -p 11904 --duration 10 --interval 300

# Print system wide stat at interval of 300ms for 10 seconds. Note that system wide profiling needs
# root privilege.
$ su 0 simpleperf stat -a --duration 10 --interval 300
```

#### Display counters in systrace

Simpleperf can also work with systrace to dump counters in the collected trace. Below is an example
to do a system wide stat.

```sh
# Capture instructions (kernel only) and cache misses with interval of 300 milliseconds for 15
# seconds.
$ su 0 simpleperf stat -e instructions:k,cache-misses -a --interval 300 --duration 15
# On host launch systrace to collect trace for 10 seconds.
(HOST)$ external/chromium-trace/systrace.py --time=10 -o new.html sched gfx view
# Open the collected new.html in browser and perf counters will be shown up.
```

### The record command

The record command is used to dump samples of the profiled processes. Each sample can contain
information like the time at which the sample was generated, the number of events since last
sample, the program counter of a thread, the call chain of a thread.

By passing options, we can select which events to use, which processes/threads to monitor,
what frequency to dump samples, how long to monitor, and where to store samples.

```sh
# Record on process 7394 for 10 seconds, using default event (cpu-cycles), using default sample
# frequency (4000 samples per second), writing records to perf.data.
$ simpleperf record -p 7394 --duration 10
simpleperf I cmd_record.cpp:316] Samples recorded: 21430. Samples lost: 0.
```

#### Select events to record

By default, the cpu-cycles event is used to evaluate consumed cpu cycles. But we can also use other
events via -e.

```sh
# Record using event instructions.
$ simpleperf record -e instructions -p 11904 --duration 10

# Record using task-clock, which shows the passed CPU time in nanoseconds.
$ simpleperf record -e task-clock -p 11904 --duration 10
```

#### Select target to record

The way to select target in record command is similar to that in the stat command.

```sh
# Record process 11904 and 11905.
$ simpleperf record -p 11904,11905 --duration 10

# Record thread 11904 and 11905.
$ simpleperf record -t 11904,11905 --duration 10

# Record a child process running `ls`.
$ simpleperf record ls

# Record a debuggable Android application.
$ simpleperf record --app com.example.simpleperf.simpleperfexamplewithnative

# Record system wide.
$ simpleperf record -a --duration 10
```

#### Set the frequency to record

We can set the frequency to dump records via -f or -c. For example, -f 4000 means
dumping approximately 4000 records every second when the monitored thread runs. If a monitored
thread runs 0.2s in one second (it can be preempted or blocked in other times), simpleperf dumps
about 4000 * 0.2 / 1.0 = 800 records every second. Another way is using -c. For example, -c 10000
means dumping one record whenever 10000 events happen.

```sh
# Record with sample frequency 1000: sample 1000 times every second running.
$ simpleperf record -f 1000 -p 11904,11905 --duration 10

# Record with sample period 100000: sample 1 time every 100000 events.
$ simpleperf record -c 100000 -t 11904,11905 --duration 10
```

#### Decide how long to record

The way to decide how long to monitor in record command is similar to that in the stat command.

```sh
# Record process 11904 for 10 seconds.
$ simpleperf record -p 11904 --duration 10

# Record until the child process running `ls` finishes.
$ simpleperf record ls

# Stop monitoring using Ctrl-C.
$ simpleperf record -p 11904 --duration 10
^C
```

If you want to write a script to control how long to monitor, you can send one of SIGINT, SIGTERM,
SIGHUP signals to simpleperf to stop monitoring.

#### Set the path to store profiling data

By default, simpleperf stores profiling data in perf.data in the current directory. But the path
can be changed using -o.

```sh
# Write records to data/perf2.data.
$ simpleperf record -p 11904 -o data/perf2.data --duration 10
```

<a name="record-call-graphs-in-record-cmd"></a>
#### Record call graphs

A call graph is a tree showing function call relations. Below is an example.

```
main() {
    FunctionOne();
    FunctionTwo();
}
FunctionOne() {
    FunctionTwo();
    FunctionThree();
}
a call graph:
    main-> FunctionOne
       |    |
       |    |-> FunctionTwo
       |    |-> FunctionThree
       |
       |-> FunctionTwo
```

A call graph shows how a function calls other functions, and a reversed call graph shows how
a function is called by other functions. To show a call graph, we need to first record it, then
report it.

There are two ways to record a call graph, one is recording a dwarf based call graph, the other is
recording a stack frame based call graph. Recording dwarf based call graphs needs support of debug
information in native binaries. While recording stack frame based call graphs needs support of
stack frame registers.

```sh
# Record a dwarf based call graph
$ simpleperf record -p 11904 -g --duration 10

# Record a stack frame based call graph
$ simpleperf record -p 11904 --call-graph fp --duration 10
```

[Here](#suggestions-about-recording-call-graphs) are some suggestions about recording call graphs.

#### Record both on CPU time and off CPU time

Simpleperf is a CPU profiler, it generates samples for a thread only when it is running on a CPU.
However, sometimes we want to figure out where the time of a thread is spent, whether it is running
on a CPU, or staying in the kernel's ready queue, or waiting for something like I/O events.

To support this, the record command uses --trace-offcpu to trace both on CPU time and off CPU time.
When --trace-offcpu is used, simpleperf generates a sample when a running thread is scheduled out,
so we know the callstack of a thread when it is scheduled out. And when reporting a perf.data
generated with --trace-offcpu, we use time to the next sample (instead of event counts from the
previous sample) as the weight of the current sample. As a result, we can get a call graph based
on timestamps, including both on CPU time and off CPU time.

trace-offcpu is implemented using sched:sched_switch tracepoint event, which may not be supported
on old kernels. But it is guaranteed to be supported on devices >= Android O MR1. We can check
whether trace-offcpu is supported as below.

```sh
$ simpleperf list --show-features
dwarf-based-call-graph
trace-offcpu
```

If trace-offcpu is supported, it will be shown in the feature list. Then we can try it.

```sh
# Record with --trace-offcpu.
$ simpleperf record -g -p 11904 --duration 10 --trace-offcpu

# Record with --trace-offcpu using app_profiler.py.
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative -a .SleepActivity \
    -r "-g -e task-clock:u -f 1000 --duration 10 --trace-offcpu"
```

Below is an example comparing the profiling result with / without --trace-offcpu.
First we record without --trace-offcpu.

```sh
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative -a .SleepActivity

$ python report_html.py --add_disassembly --add_source_code --source_dirs ../demo
```

The result is [here](./without_trace_offcpu.html).
In the result, all time is taken by RunFunction(), and sleep time is ignored.
But if we add --trace-offcpu, the result changes.

```sh
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative -a .SleepActivity \
    -r "-g -e task-clock:u --trace-offcpu -f 1000 --duration 10"

$ python report_html.py --add_disassembly --add_source_code --source_dirs ../demo
```

The result is [here](./trace_offcpu.html).
In the result, half of the time is taken by RunFunction(), and the other half is taken by
SleepFunction(). So it traces both on CPU time and off CPU time.

### The report command

The report command is used to report profiling data generated by the record command. The report
contains a table of sample entries. Each sample entry is a row in the report. The report command
groups samples belong to the same process, thread, library, function in the same sample entry. Then
sort the sample entries based on the event count a sample entry has.

By passing options, we can decide how to filter out uninteresting samples, how to group samples
into sample entries, and where to find profiling data and binaries.

Below is an example. Records are grouped into 4 sample entries, each entry is a row. There are
several columns, each column shows piece of information belonging to a sample entry. The first
column is Overhead, which shows the percentage of events inside the current sample entry in total
events. As the perf event is cpu-cycles, the overhead is the percentage of CPU cycles used in each
function.

```sh
# Reports perf.data, using only records sampled in libsudo-game-jni.so, grouping records using
# thread name(comm), process id(pid), thread id(tid), function name(symbol), and showing sample
# count for each row.
$ simpleperf report --dsos /data/app/com.example.sudogame-2/lib/arm64/libsudo-game-jni.so \
      --sort comm,pid,tid,symbol -n
Cmdline: /data/data/com.example.sudogame/simpleperf record -p 7394 --duration 10
Arch: arm64
Event: cpu-cycles (type 0, config 0)
Samples: 28235
Event count: 546356211

Overhead  Sample  Command    Pid   Tid   Symbol
59.25%    16680   sudogame  7394  7394  checkValid(Board const&, int, int)
20.42%    5620    sudogame  7394  7394  canFindSolution_r(Board&, int, int)
13.82%    4088    sudogame  7394  7394  randomBlock_r(Board&, int, int, int, int, int)
6.24%     1756    sudogame  7394  7394  @plt
```

#### Set the path to read profiling data

By default, the report command reads profiling data from perf.data in the current directory.
But the path can be changed using -i.

```sh
$ simpleperf report -i data/perf2.data
```

#### Set the path to find binaries

To report function symbols, simpleperf needs to read executable binaries used by the monitored
processes to get symbol table and debug information. By default, the paths are the executable
binaries used by monitored processes while recording. However, these binaries may not exist when
reporting or not contain symbol table and debug information. So we can use --symfs to redirect
the paths.

```sh
# In this case, when simpleperf wants to read executable binary /A/b, it reads file in /A/b.
$ simpleperf report

# In this case, when simpleperf wants to read executable binary /A/b, it prefers file in
# /debug_dir/A/b to file in /A/b.
$ simpleperf report --symfs /debug_dir
```

#### Filter samples

When reporting, it happens that not all records are of interest. The report command supports four
filters to select samples of interest.

```sh
# Report records in threads having name sudogame.
$ simpleperf report --comms sudogame

# Report records in process 7394 or 7395
$ simpleperf report --pids 7394,7395

# Report records in thread 7394 or 7395.
$ simpleperf report --tids 7394,7395

# Report records in libsudo-game-jni.so.
$ simpleperf report --dsos /data/app/com.example.sudogame-2/lib/arm64/libsudo-game-jni.so
```

#### Group samples into sample entries

The report command uses --sort to decide how to group sample entries.

```sh
# Group records based on their process id: records having the same process id are in the same
# sample entry.
$ simpleperf report --sort pid

# Group records based on their thread id and thread comm: records having the same thread id and
# thread name are in the same sample entry.
$ simpleperf report --sort tid,comm

# Group records based on their binary and function: records in the same binary and function are in
# the same sample entry.
$ simpleperf report --sort dso,symbol

# Default option: --sort comm,pid,tid,dso,symbol. Group records in the same thread, and belong to
# the same function in the same binary.
$ simpleperf report
```

<a name="report-call-graphs-in-report-cmd"></a>
#### Report call graphs

To report a call graph, please make sure the profiling data is recorded with call graphs,
as [here](#record-call-graphs-in-record-cmd).

```
$ simpleperf report -g
```

## Scripts reference

<a name="app_profiler-py"></a>
### app_profiler.py

app_profiler.py is used to record profiling data for Android applications and native executables.

```sh
# Record an Android application.
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative

# Record an Android application without compiling the Java code into native instructions.
# Used when you only profile the C++ code, or the Java code has already been compiled into native
# instructions.
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative -nc

# Record running a specific activity of an Android application.
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative -a .SleepActivity

# Record a native process.
$ python app_profiler.py -np surfaceflinger

# Record a command.
$ python app_profiler.py -cmd \
    "dex2oat --dex-file=/data/local/tmp/app-profiling.apk --oat-file=/data/local/tmp/a.oat" \
    --arch arm

# Record an Android application, and use -r to send custom options to the record command.
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative \
    -r "-e cpu-clock -g --duration 30"

# Record both on CPU time and off CPU time.
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative \
    -r "-e task-clock -g -f 1000 --duration 10 --trace-offcpu"

# Profile activity startup time using --profile_from_launch.
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative \
    --profile_from_launch --arch arm64
```

#### Profile from launch of an application

Sometimes we want to profile the launch-time of an application. To support this, we added --app in
the record command. The --app option sets the package name of the Android application to profile.
If the app is not already running, the record command will poll for the app process in a loop with
an interval of 1ms. So to profile from launch of an application, we can first start the record
command with --app, then start the app. Below is an example.

```sh
$ python run_simpleperf_on_device.py record
    --app com.example.simpleperf.simpleperfexamplewithnative \
    -g --duration 1 -o /data/local/tmp/perf.data
# Start the app manually or using the `am` command.
```

To make it convenient to use, app_profiler.py combines these in the --profile_from_launch option.

```sh
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative -a .MainActivity \
    --arch arm64 --profile_from_launch
```

<a name="binary_cache_builder-py"></a>
### binary_cache_builder.py

The binary_cache directory is a directory holding binaries needed by a profiling data file. The
binaries are expected to be unstripped, having debug information and symbol tables. The
binary_cache directory is used by report scripts to read symbols of binaries. It is also used by
report_html.py to generate annotated source code and disassembly.

By default, app_profiler.py builds the binary_cache directory after recording. But we can also
build binary_cache for existing profiling data files using binary_cache_builder.py. It is useful
when you record profiling data using `simpleperf record` directly, to do system wide profiling or
record without usb cable connected.

binary_cache_builder.py can either pull binaries from an Android device, or find binaries in
directories on the host (via -lib).

```sh
# Generate binary_cache for perf.data, by pulling binaries from the device.
$ python binary_cache_builder.py

# Generate binary_cache, by pulling binaries from the device and finding binaries in ../demo.
$ python binary_cache_builder.py -lib ../demo
```

<a name="run_simpleperf_on_device-py"></a>
### run_simpleperf_on_device.py

This script pushes the simpleperf executable on the device, and run a simpleperf command on the
device. It is more convenient than running adb commands manually.

<a name="report-py"></a>
### report.py

report.py is a wrapper of the report command on the host. It accepts all options of the report
command.

```sh
# Report call graph
$ python report.py -g

# Report call graph in a GUI window implemented by Python Tk.
$ python report.py -g --gui
```

<a name="report_html-py"></a>
### report_html.py

report_html.py generates report.html based on the profiling data. Then the report.html can show
the profiling result without depending on other files. So it can be shown in local browsers or
passed to other machines. Depending on which command-line options are used, the content of the
report.html can include: chart statistics, sample table, flame graphs, annotated source code for
each function, annotated disassembly for each function.

```sh
# Generate chart statistics, sample table and flame graphs, based on perf.data.
$ python report_html.py

# Add source code.
$ python report_html.py --add_source_code --source_dirs ../demo/SimpleperfExampleWithNative

# Add disassembly.
$ python report_html.py --add_disassembly
```

Below is an example of generating html profiling results for SimpleperfExampleWithNative.

```sh
$ python app_profiler.py -p com.example.simpleperf.simpleperfexamplewithnative
$ python report_html.py --add_source_code --source_dirs ../demo --add_disassembly
```

After opening the generated [report.html](./report_html.html) in a browser, there are several tabs:

The first tab is "Chart Statistics". You can click the pie chart to show the time consumed by each
process, thread, library and function.

The second tab is "Sample Table". It shows the time taken by each function. By clicking one row in
the table, we can jump to a new tab called "Function".

The third tab is "Flamegraph". It shows the flame graphs generated by [inferno](./inferno.md).

The fourth tab is "Function". It only appears when users click a row in the "Sample Table" tab.
It shows information of a function, including:

1. A flame graph showing functions called by that function.
2. A flame graph showing functions calling that function.
3. Annotated source code of that function. It only appears when there are source code files for
   that function.
4. Annotated disassembly of that function. It only appears when there are binaries containing that
   function.

### inferno

[inferno](./inferno.md) is a tool used to generate flame graph in a html file.

```sh
# Generate flame graph based on perf.data.
# On Windows, use inferno.bat instead of ./inferno.sh.
$ ./inferno.sh -sc --record_file perf.data

# Record a native program and generate flame graph.
$ ./inferno.sh -np surfaceflinger
```

<a name="pprof_proto_generator-py"></a>
### pprof_proto_generator.py

It converts a profiling data file into pprof.proto, a format used by [pprof](https://github.com/google/pprof).

```sh
# Convert perf.data in the current directory to pprof.proto format.
$ python pprof_proto_generator.py
$ pprof -pdf pprof.profile
```

<a name="report_sample-py"></a>
### report_sample.py

It converts a profiling data file into a format used by [FlameGraph](https://github.com/brendangregg/FlameGraph).

```sh
# Convert perf.data in the current directory to a format used by FlameGraph.
$ python report_sample.py --symfs binary_cache >out.perf
$ git clone https://github.com/brendangregg/FlameGraph.git
$ FlameGraph/stackcollapse-perf.pl out.perf >out.folded
$ FlameGraph/flamegraph.pl out.folded >a.svg
```

<a name="simpleperf_report_lib-py"></a>
### simpleperf_report_lib.py

simpleperf_report_lib.py is a Python library used to parse profiling data files generated by the
record command. Internally, it uses libsimpleperf_report.so to do the work. Generally, for each
profiling data file, we create an instance of ReportLib, pass it the file path (via SetRecordFile).
Then we can read all samples through GetNextSample(). For each sample, we can read its event info
(via GetEventOfCurrentSample), symbol info (via GetSymbolOfCurrentSample) and call chain info
(via GetCallChainOfCurrentSample). We can also get some global information, like record options
(via GetRecordCmd), the arch of the device (via GetArch) and meta strings (via MetaInfo).

Examples of using simpleperf_report_lib.py are in report_sample.py, report_html.py,
pprof_proto_generator.py and inferno/inferno.py.

## Answers to common issues

### Why we suggest profiling on Android >= N devices?
```
1. Running on a device reflects a real running situation, so we suggest
profiling on real devices instead of emulators.
2. To profile Java code, we need ART running in oat mode, which is only
available >= L for rooted devices, and >= N for non-rooted devices.
3. Old Android versions are likely to be shipped with old kernels (< 3.18),
which may not support profiling features like recording dwarf based call graphs.
4. Old Android versions are likely to be shipped with Arm32 chips. In Arm32
mode, recording stack frame based call graphs doesn't work well.
```

### Suggestions about recording call graphs

Below is our experiences of dwarf based call graphs and stack frame based call graphs.

dwarf based call graphs:
1. Need support of debug information in binaries.
2. Behave normally well on both ARM and ARM64, for both fully compiled Java code and C++ code.
3. Can only unwind 64K stack for each sample. So usually can't show complete flame-graph. But
   probably is enough for users to identify hot places.
4. Take more CPU time than stack frame based call graphs. So the sample frequency is suggested
   to be 1000 Hz. Thus at most 1000 samples per second.

stack frame based call graphs:
1. Need support of stack frame registers.
2. Don't work well on ARM. Because ARM is short of registers, and ARM and THUMB code have different
   stack frame registers. So the kernel can't unwind user stack containing both ARM/THUMB code.
3. Also don't work well on fully compiled Java code on ARM64. Because the ART compiler doesn't
   reserve stack frame registers.
4. Work well when profiling native programs on ARM64. One example is profiling surfacelinger. And
   usually shows complete flame-graph when it works well.
5. Take less CPU time than dwarf based call graphs. So the sample frequency can be 4000 Hz or
   higher.

So if you need to profile code on ARM or profile fully compiled Java code, dwarf based call graphs
may be better. If you need to profile C++ code on ARM64, stack frame based call graphs may be
better. After all, you can always try dwarf based call graph first, because it always produces
reasonable results when given unstripped binaries properly. If it doesn't work well enough, then
try stack frame based call graphs instead.

Simpleperf needs to have unstripped native binaries on the device to generate good dwarf based call
graphs. It can be supported in two ways:
1. Use unstripped native binaries when building the apk, as [here](https://android.googlesource.com/platform/system/extras/+/master/simpleperf/demo/SimpleperfExampleWithNative/app/profiling.gradle).
2. Pass directory containing unstripped native libraries to app_profiler.py via -lib. And it will
   download the unstripped native libraries on the device.

```sh
$ python app_profiler.py -lib NATIVE_LIB_DIR
```
