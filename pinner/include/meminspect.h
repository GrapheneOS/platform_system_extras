#pragma once

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

#define MEMINSPECT_FAIL_OPEN 1
#define MEMINSPECT_FAIL_FSTAT 2
#define MEMINSPECT_FAIL_MINCORE 3

#define DEFAULT_PAGES_PER_MINCORE 1

/**
 * This class stores an offset defined vma which exists
 * relative to another memory address.
 */
class VmaRange {
  public:
    uint32_t offset;
    uint32_t length;

    VmaRange(uint32_t off, uint32_t len) : offset(off), length(len) {}
};

struct ResidentMemResult {
  public:
    std::vector<VmaRange> resident_memory_ranges;
    uint64_t file_size_bytes;
    uint64_t total_resident_bytes;
};

/**
 * @brief Probe resident memory for a currently opened file in the system.
 *
 * @param probed_file File to probe as defined by its path.
 * @param out_resident_mem Inspection result. This is populated when called.
 * @param pages_per_mincore Size of mincore window used, bigger means more memory but slightly
 * faster.
 * @return 0 on success or on failure a non-zero error code from the following list:
 * MEMINSPECT_FAIL_OPEN, MEMINSPECT_FAIL_FSTAT, MEMINSPECT_FAIL_MINCORE
 */
int probe_resident_memory(std::string probed_file, ResidentMemResult& out_resident_mem,
                          int pages_per_mincore = DEFAULT_PAGES_PER_MINCORE);