#include <android-base/stringprintf.h>
#include <fcntl.h>
#include <sys/endian.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>

#include <meminspect.h>
#include <pin_utils.h>

using namespace std;
using namespace android::base;

enum ToolMode { PROBE, DUMP, UNKNOWN };

void print_pinner_ranges(const std::vector<VmaRange>& ranges) {
    cout << "vmas to pin:" << endl;
    for (int i = 0; i < ranges.size(); ++i) {
        cout << StringPrintf("start=%u length=%u", ranges[i].offset, ranges[i].length) << endl;
    }
}

int perform_probe(const vector<string>& options) {
    std::string probed_file;
    std::string output_file = "/data/local/tmp/pinlist.meta";
    bool verbose = false;
    int pages_per_mincore = DEFAULT_PAGES_PER_MINCORE;

    // Parse Args
    for (int i = 0; i < options.size(); ++i) {
        string option = options[i];
        if (option == "-p") {
            ++i;
            probed_file = options[i];
            continue;
        }
        if (option == "-o") {
            ++i;
            output_file = options[i];
            continue;
        }
        if (option == "-v") {
            verbose = true;
            continue;
        }
        if (option == "-w") {
            ++i;
            pages_per_mincore = stoi(options[i]);
            continue;
        }
    }

    if (verbose) {
        cout << "mincore window size=" << pages_per_mincore << endl;
        cout << "Setting output pinlist file as " << probed_file.c_str() << endl;
        cout << "Setting file to probe: " << probed_file.c_str() << endl;
    }

    if (probed_file.empty()) {
        cerr << "Error: Should specify a file to probe.";
        return 1;
    }

    ResidentMemResult memresult;
    int res = probe_resident_memory(probed_file, memresult, pages_per_mincore);
    if (res) {
        if (res == MEMINSPECT_FAIL_OPEN) {
            cerr << "Failed to open file: " << probed_file << endl;
        }

        if (res == MEMINSPECT_FAIL_FSTAT) {
            cerr << "Failed to fstat file: " << probed_file << endl;
        }

        if (res == MEMINSPECT_FAIL_MINCORE) {
            cerr << "Mincore failed for file: " << probed_file << endl;
        }

        return res;
    }

    cout << StringPrintf(
                    "Finished Probing. resident memory(KB)=%lu. file_size (KB)=%lu. "
                    "pin_percentage=%f",
                    memresult.total_resident_bytes / 1024, memresult.file_size_bytes / 1024,
                    memresult.total_resident_bytes / (float)memresult.file_size_bytes * 100)
         << endl;

    res = write_pinlist_file(output_file, memresult.resident_memory_ranges);
    if (res > 0) {
        cerr << "Failed to write pin file at: " << output_file << endl;
    } else {
        if (verbose) {
            cout << "Finished writing pin file at: " << output_file << endl;
        }
    }
    return res;
}

int perform_dump(const vector<string>& options) {
    string pinner_file;
    bool verbose = false;
    for (int i = 0; i < options.size(); ++i) {
        string option = options[i];
        if (option == "-p") {
            ++i;
            pinner_file = options[i];
            continue;
        }

        if (option == "-v") {
            verbose = true;
        }
    }
    if (pinner_file.empty()) {
        cerr << "Error: Pinlist file to dump is missing. Specify it with '-p <file>'" << endl;
        return 1;
    }
    if (verbose) {
        cout << "Setting file to dump: " << pinner_file.c_str() << endl;
    }
    vector<VmaRange> vma_ranges;
    if (read_pinlist_file(pinner_file, vma_ranges) == 1) {
        cerr << "Failed reading pinlist file" << endl;
    }
    print_pinner_ranges(vma_ranges);

    return 0;
}

int main(int argc, char** argv) {
    if (argc == 1) {
        const string usage = R"(
Expected usage: pintool <mode> <required> [option]
where:
<file_to_pin> is a file currently mapped by another process and in memory.
<mode> :
    probe
        This mode will probe resident memory for a file and generate a pinlist.meta file
        that can be interpreted by the PinnerService.

        <required>
            -p <file_to_probe>
                This option will probe the specified file
        [option]:
            -o <file>
                Specify the output file for the pinlist file.
                (default=/data/local/tmp/pinlist.meta)
            -v
                Enable verbose output.
            -w
                Mincore total pages per mincore window. Bigger windows
                will use more memory but may be slightly faster. (default=1)
    dump
        <required>
            -p <input_pinlist_file>
                Specify the input pinlist file to dump
)";

        cout << usage.c_str();
        return 0;
    }

    if (argc < 2) {
        cerr << "<mode> is missing";
        return 1;
    }
    ToolMode mode = UNKNOWN;
    if (strcmp(argv[1], "probe") == 0) {
        mode = PROBE;
    } else if (strcmp(argv[1], "dump") == 0) {
        mode = DUMP;
    }

    if (mode == UNKNOWN) {
        cerr << "Failed to find mode: " << argv[1] << ". See usage for available modes." << endl;
        return 1;
    }

    vector<string> options;
    for (int i = 2; i < argc; ++i) {
        options.push_back(argv[i]);
    }

    int res;
    switch (mode) {
        case PROBE:
            res = perform_probe(options);
            break;
        case DUMP:
            res = perform_dump(options);
            break;
        case UNKNOWN:
            return 1;
            break;
    }

    return res;
}
