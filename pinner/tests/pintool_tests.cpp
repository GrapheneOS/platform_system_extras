#include <gtest/gtest.h>
#include <stdio.h>

#include <pin_utils.h>

using namespace std;

TEST(pintool_test, pinlist_matches_memranges) {
    vector<VmaRange> vma_ranges;
    vma_ranges.push_back(VmaRange(0, 500));
    vma_ranges.push_back(VmaRange(1000, 5500));
    vma_ranges.push_back(VmaRange(10000, 13000));
    vma_ranges.push_back(VmaRange(26000, 35000));

    string test_file = "/data/local/tmp/pintool_test";
    write_pinlist_file(test_file, vma_ranges);

    vector<VmaRange> read_ranges;
    read_pinlist_file(test_file, read_ranges);

    EXPECT_EQ(vma_ranges.size(), read_ranges.size());
    for (size_t i = 0; i < vma_ranges.size(); ++i) {
        EXPECT_EQ(vma_ranges[i].offset, read_ranges[i].offset);
        EXPECT_EQ(vma_ranges[i].length, read_ranges[i].length);
    }

    remove(test_file.c_str());
}