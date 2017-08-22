#
# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""
    Inferno is a tool to generate flamegraphs for android programs. It was originally written
    to profile surfaceflinger (Android compositor) but it can be used for other C++ program.
    It uses simpleperf to collect data. Programs have to be compiled with frame pointers which
    excludes ART based programs for the time being.

    Here is how it works:

    1/ Data collection is started via simpleperf and pulled locally as "perf.data".
    2/ The raw format is parsed, callstacks are merged to form a flamegraph data structure.
    3/ The data structure is used to generate a SVG embedded into an HTML page.
    4/ Javascript is injected to allow flamegraph navigation, search, coloring model.

"""

import argparse
import datetime
import os
import subprocess
import sys
import webbrowser

try:
    from simpleperf_report_lib import ReportLib
    from utils import log_exit, log_info, AdbHelper
except:
    print("Please go to the parent directory, and run inferno.sh or inferno.bat.")
    sys.exit(1)

from data_types import *
from svg_renderer import *


def collect_data(args):
    app_profiler_args = [sys.executable, "app_profiler.py", "-nb"]
    if args.app:
        app_profiler_args += ["-p", args.app]
    elif args.native_program:
        app_profiler_args += ["-np", args.native_program]
    else:
        log_exit("Please set profiling target with -p or -np option.")
    if args.skip_recompile:
        app_profiler_args.append("-nc")
    if args.disable_adb_root:
        app_profiler_args.append("--disable_adb_root")
    record_arg_str = ""
    if args.dwarf_unwinding:
        record_arg_str += "-g "
    else:
        record_arg_str += "--call-graph fp "
    if args.events:
        tokens = args.events.split()
        if len(tokens) == 2:
            num_events = tokens[0]
            event_name = tokens[1]
            record_arg_str += "-c %s -e %s " % (num_events, event_name)
        else:
            log_exit("Event format string of -e option cann't be recognized.")
        log_info("Using event sampling (-c %s -e %s)." % (num_events, event_name))
    else:
        record_arg_str += "-f %d " % args.sample_frequency
        log_info("Using frequency sampling (-f %d)." % args.sample_frequency)
    record_arg_str += "--duration %d " % args.capture_duration
    app_profiler_args += ["-r", record_arg_str]
    returncode = subprocess.call(app_profiler_args)
    return returncode == 0


def parse_samples(process, args):
    """ read record_file, and print each sample"""

    record_file = args.record_file
    symfs_dir = args.symfs
    kallsyms_file = args.kallsyms

    lib = ReportLib()

    lib.ShowIpForUnknownSymbol()
    if symfs_dir:
        lib.SetSymfs(symfs_dir)
    if record_file:
        lib.SetRecordFile(record_file)
    if kallsyms_file:
        lib.SetKallsymsFile(kallsyms_file)
    process.cmd = lib.GetRecordCmd()
    product_props = lib.MetaInfo().get("product_props")
    if product_props:
        tuple = product_props.split(':')
        process.props['ro.product.manufacturer'] = tuple[0]
        process.props['ro.product.model'] = tuple[1]
        process.props['ro.product.name'] = tuple[2]

    while True:
        sample = lib.GetNextSample()
        if sample is None:
            lib.Close()
            break
        symbol = lib.GetSymbolOfCurrentSample()
        callchain = lib.GetCallChainOfCurrentSample()
        process.get_thread(sample.tid, sample.pid).add_callchain(callchain, symbol, sample)
        process.num_samples += 1

    if process.pid == 0:
        main_threads = [thread for thread in process.threads.values() if thread.tid == thread.pid]
        if main_threads:
            process.name = main_threads[0].name
            process.pid = main_threads[0].pid

    log_info("Parsed %s callchains." % process.num_samples)


def get_local_asset_content(local_path):
    """
    Retrieves local package text content
    :param local_path: str, filename of local asset
    :return: str, the content of local_path
    """
    with open(os.path.join(os.path.dirname(__file__), local_path), 'r') as f:
        return f.read()


def output_report(process, args):
    """
    Generates a HTML report representing the result of simpleperf sampling as flamegraph
    :param process: Process object
    :return: str, absolute path to the file
    """
    f = open('report.html', 'w')
    filepath = os.path.realpath(f.name)
    f.write("<html>")
    f.write("<head>")
    f.write("""<style type="text/css">""")
    f.write(get_local_asset_content(os.path.join("jqueryui", "jquery-ui.min.css")))
    f.write("</style>")
    f.write("</head>")
    f.write("<body style='font-family: Monospace;' onload='init()'>")
    f.write("""<style type="text/css"> .s { stroke:black; stroke-width:0.5; cursor:pointer;}
            </style>""")
    f.write('<style type="text/css"> .t:hover { cursor:pointer; } </style>')
    f.write('<img height="180" alt = "Embedded Image" src ="data')
    f.write(get_local_asset_content("inferno.b64"))
    f.write('"/>')
    process_entry = ("Process : %s (%d)<br/>" % (process.name, process.pid)) if process.pid else ""
    # TODO: collect capture duration info from perf.data.
    duration_entry = ("Duration: %s seconds<br/>" % args.capture_duration
                      ) if args.capture_duration else ""
    f.write("""<div style='display:inline-block;'>
                  <font size='8'>
                  Inferno Flamegraph Report</font><br/><br/>
                  %s
                  Date&nbsp;&nbsp;&nbsp;&nbsp;: %s<br/>
                  Threads : %d <br/>
                  Samples : %d</br>
                  %s""" % (
        process_entry,
        datetime.datetime.now().strftime("%Y-%m-%d (%A) %H:%M:%S"),
        len(process.threads),
        process.num_samples,
        duration_entry))
    if 'ro.product.model' in process.props:
        f.write(
            "Machine : %s (%s) by %s<br/>" %
            (process.props["ro.product.model"],
             process.props["ro.product.name"],
             process.props["ro.product.manufacturer"]))
    if process.cmd:
        f.write("Capture : %s<br/><br/>" % process.cmd)
    f.write("</div>")
    f.write("""<br/><br/>
            <div>Navigate with WASD, zoom in with SPACE, zoom out with BACKSPACE.</div>""")
    f.write("<script>")
    f.write(get_local_asset_content(os.path.join("jqueryui", "jquery-3.2.1.min.js")))
    f.write(get_local_asset_content(os.path.join("jqueryui", "jquery-ui.min.js")))
    f.write("</script>")
    f.write("<script>%s</script>" % get_local_asset_content("script.js"))

    # Output tid == pid Thread first.
    main_thread = [x for x in process.threads.values() if x.tid == process.pid]
    for thread in main_thread:
        f.write("<br/><br/><b>Main Thread %d (%s) (%d samples):</b><br/>\n\n\n\n" % (
                thread.tid, thread.name, thread.num_samples))
        renderSVG(thread.flamegraph, f, args.color, args.svg_width)

    other_threads = [x for x in process.threads.values() if x.tid != process.pid]
    for thread in other_threads:
        f.write("<br/><br/><b>Thread %d (%s) (%d samples):</b><br/>\n\n\n\n" % (
                thread.tid, thread.name, thread.num_samples))
        renderSVG(thread.flamegraph, f, args.color, args.svg_width)

    f.write("</body>")
    f.write("</html>")
    f.close()
    return "file://" + filepath


def generate_threads_offsets(process):
    for thread in process.threads.values():
       thread.flamegraph.generate_offset(0)


def collect_machine_info(process):
    adb = AdbHelper()
    process.props = {}
    process.props['ro.product.model'] = adb.get_property('ro.product.model')
    process.props['ro.product.name'] = adb.get_property('ro.product.name')
    process.props['ro.product.manufacturer'] = adb.get_property('ro.product.manufacturer')


def open_report_in_browser(report_path):
    try:
        # Try to open the report with Chrome
        browser_key = ""
        for key, value in webbrowser._browsers.items():
            if key.find("chrome") != -1:
                browser_key = key
        browser = webbrowser.get(browser_key)
        browser.open(report_path, new=0, autoraise=True)
    except:
        # webbrowser.get() doesn't work well on darwin/windows.
        webbrowser.open_new_tab(report_path)


def main():

    parser = argparse.ArgumentParser(description='Report samples in perf.data.')
    parser.add_argument('--symfs', help="""Set the path to find binaries with symbols and debug
                        info.""")
    parser.add_argument('--kallsyms', help='Set the path to find kernel symbols.')
    parser.add_argument('--record_file', default='perf.data', help='Default is perf.data.')
    parser.add_argument('-t', '--capture_duration', type=int, default=10,
                        help="""Capture duration in seconds.""")
    parser.add_argument('-p', '--app', help="""Profile an Android app, given the package name.
                        Like -p com.example.android.myapp.""")
    parser.add_argument('-np', '--native_program', default="surfaceflinger",
                        help="""Profile a native program. The program should be running on the
                        device. Like -np surfaceflinger.""")
    parser.add_argument('-c', '--color', default='hot', choices=['hot', 'dso', 'legacy'],
                        help="""Color theme: hot=percentage of samples, dso=callsite DSO name,
                        legacy=brendan style""")
    parser.add_argument('-sc', '--skip_collection', default=False, help='Skip data collection',
                        action="store_true")
    parser.add_argument('-nc', '--skip_recompile', action='store_true', help="""When profiling
                        an Android app, by default we recompile java bytecode to native
                        instructions to profile java code. It takes some time. You can skip it
                        if the code has been compiled or you don't need to profile java code.""")
    parser.add_argument('-f', '--sample_frequency', type=int, default=6000, help='Sample frequency')
    parser.add_argument('-w', '--svg_width', type=int, default=1124)
    parser.add_argument(
        '-du',
        '--dwarf_unwinding',
        help='Perform unwinding using dwarf instead of fp.',
        default=False,
        action='store_true')
    parser.add_argument('-e', '--events', help="""Sample based on event occurences instead of
                        frequency. Format expected is "event_counts event_name".
                        e.g: "10000 cpu-cyles". A few examples of event_name: cpu-cycles,
                        cache-references, cache-misses, branch-instructions, branch-misses""",
                        default="")
    parser.add_argument('--disable_adb_root', action='store_true', help="""Force adb to run in non
                        root mode.""")
    args = parser.parse_args()
    process = Process("", 0)

    if not args.skip_collection:
        process.name = args.app or args.native_program
        log_info("Starting data collection stage for process '%s'." % process.name)
        if not collect_data(args):
            log_exit("Unable to collect data.")
        result, output = AdbHelper().run_and_return_output(['shell', 'pidof', process.name])
        if result:
            try:
                process.pid = int(output)
            except:
                process.pid = 0
        collect_machine_info(process)
    else:
        args.capture_duration = 0

    parse_samples(process, args)
    generate_threads_offsets(process)
    report_path = output_report(process, args)
    open_report_in_browser(report_path)

    log_info("Report generated at '%s'." % report_path)

if __name__ == "__main__":
    main()
