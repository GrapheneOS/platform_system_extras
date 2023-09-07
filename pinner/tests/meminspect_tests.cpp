#include <gtest/gtest.h>
#include <stdio.h>

#include <meminspect.h>

using namespace std;

/**
 * This test is meant to be ran by directly pushing the test binary
 * into the device as using atest will not provide sufficient privileges
 * to execute drop_caches command.
 */
TEST(meminspect_test, inspect_matches_resident) {
    // Create test file
    string test_file = "/data/local/tmp/meminspect_test";
    // If for any reason a test file already existed from a previous test, remove it.
    remove(test_file.c_str());

    int test_file_fd = open(test_file.c_str(), O_RDWR | O_CREAT, "w");
    unsigned int page_size = sysconf(_SC_PAGESIZE);
    if (test_file_fd == -1) {
        ADD_FAILURE() << "Failed to open test file for writing. errno: " << std::strerror(errno);
        close(test_file_fd);
        remove(test_file.c_str());
        return;
    }

    uint8_t* page_data = new uint8_t[page_size];
    for (unsigned int i = 0; i < page_size; ++i) {
        page_data[i] = i + 1;
    }
    int pages_to_write = 100;
    for (int page = 0; page < pages_to_write; ++page) {
        write(test_file_fd, page_data, page_size);
    }
    // fsync to ensure the data is flushed to disk.
    if (fsync(test_file_fd) == -1) {
        ADD_FAILURE() << "fsync failed errno: " << std::strerror(errno);
        close(test_file_fd);
        remove(test_file.c_str());
        return;
    }
    close(test_file_fd);

    // Drop the pagecache to ensure we do not have memory due to it staying there after write.
    int drop_cache_fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
    if (drop_cache_fd == -1) {
        ADD_FAILURE() << "failed opening drop caches fd errno: " << std::strerror(errno);
        close(test_file_fd);
        remove(test_file.c_str());
        return;
    }
    write(drop_cache_fd, "3", 1);
    fsync(drop_cache_fd);
    close(drop_cache_fd);

    // Open file and page in some memory
    test_file_fd = open(test_file.c_str(), O_RDONLY, "r");
    if (test_file_fd == -1) {
        ADD_FAILURE() << "Failed to open test file for reading after creation. errno: "
                      << std::strerror(errno);
        close(test_file_fd);
        remove(test_file.c_str());
        return;
    }

    char* base_address = (char*)mmap(0, page_size * pages_to_write, PROT_READ, MAP_SHARED,
                                     test_file_fd, /*offset*/ 0);
    if (base_address == (char*)-1) {
        ADD_FAILURE() << "Failed to mmap file for reading after creation. errno: "
                      << std::strerror(errno);
        close(test_file_fd);
        remove(test_file.c_str());
        return;
    }

    ResidentMemResult vmas_resident;
    int res = probe_resident_memory(test_file, vmas_resident, 1);
    EXPECT_TRUE(res == 0);

    // Probing the file without reading anything yields no resident memory
    EXPECT_TRUE(vmas_resident.resident_memory_ranges.empty());

    // Clear our to start fresh for next probe.
    vmas_resident = ResidentMemResult();

    int pages_to_read = 1;
    char* read_data = new char[pages_to_read];
    for (int page = 0; page < pages_to_read; ++page) {
        // Read 1 byte from each page to page it in.
        read_data[page] = base_address[page * page_size];
    }
    res = probe_resident_memory(test_file, vmas_resident, 1);
    EXPECT_TRUE(res == 0);

    // The amount of memory paged in is outside our control, but we should have some.
    EXPECT_TRUE(vmas_resident.total_resident_bytes > 0);
    EXPECT_TRUE(vmas_resident.resident_memory_ranges.size() == 1);
    EXPECT_TRUE(vmas_resident.resident_memory_ranges[0].offset == 0);
    EXPECT_TRUE((uint64_t)vmas_resident.resident_memory_ranges[0].length ==
                vmas_resident.total_resident_bytes);

    close(test_file_fd);
    remove(test_file.c_str());
}