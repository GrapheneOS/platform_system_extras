#include "meminspect.h"
#include <android-base/unique_fd.h>

using namespace std;
using namespace android::base;
using namespace ::android::base;

int probe_resident_memory(string probed_file,
                          /*out*/ ResidentMemResult& memresult, int pages_per_mincore) {
    unique_fd probed_file_ufd(open(probed_file.c_str(), O_RDONLY));
    int probe_fd = probed_file_ufd.get();
    if (probe_fd == -1) {
        return MEMINSPECT_FAIL_OPEN;
    }

    struct stat fstat_res;
    int res = fstat(probe_fd, &fstat_res);
    if (res == -1) {
        return MEMINSPECT_FAIL_FSTAT;
    }
    unsigned long total_bytes = fstat_res.st_size;

    memresult.file_size_bytes = total_bytes;

    char* base_address = (char*)mmap(0, total_bytes, PROT_READ, MAP_SHARED, probe_fd, /*offset*/ 0);

    // this determines how many pages to inspect per mincore syscall
    unsigned char* window = new unsigned char[pages_per_mincore];

    unsigned int page_size = sysconf(_SC_PAGESIZE);
    unsigned long bytes_inspected = 0;
    unsigned long pages_in_memory = 0;

    // total bytes in inspection window
    unsigned long window_bytes = page_size * pages_per_mincore;

    char* current_window_address;
    bool started_vma_range = false;
    uint32_t resident_vma_start_offset = 0;
    for (current_window_address = base_address; bytes_inspected < total_bytes;
         current_window_address += window_bytes, bytes_inspected += window_bytes) {
        int res = mincore(current_window_address, window_bytes, window);
        if (res != 0) {
            if (errno == ENOMEM) {
                // Did not find page, maybe it's a hole.
                continue;
            }
            return MEMINSPECT_FAIL_MINCORE;
        }
        // Inspect the provided mincore window result sequentially
        // and as soon as a change in residency happens a range is
        // created or finished.
        for (int iWin = 0; iWin < pages_per_mincore; ++iWin) {
            if ((window[iWin] & 1) > 0) {
                // Page is resident
                ++pages_in_memory;
                if (!started_vma_range) {
                    // End of range
                    started_vma_range = true;
                    uint32_t window_offset = iWin * page_size;
                    resident_vma_start_offset =
                            current_window_address + window_offset - base_address;
                }
            } else {
                // Page is not resident
                if (started_vma_range) {
                    // Start of range
                    started_vma_range = false;
                    uint32_t window_offset = iWin * page_size;
                    uint32_t resident_vma_end_offset =
                            current_window_address + window_offset - base_address;
                    uint32_t resident_len = resident_vma_end_offset - resident_vma_start_offset;
                    VmaRange vma_range(resident_vma_start_offset, resident_len);
                    memresult.resident_memory_ranges.push_back(vma_range);
                }
            }
        }
    }
    // This was the last window, so close any opened vma range
    if (started_vma_range) {
        started_vma_range = false;
        uint32_t in_memory_vma_end = current_window_address - base_address;
        uint32_t resident_len = in_memory_vma_end - resident_vma_start_offset;
        VmaRange vma_range(resident_vma_start_offset, resident_len);
        memresult.resident_memory_ranges.push_back(vma_range);
    }

    memresult.total_resident_bytes = pages_in_memory * page_size;
    return 0;
}