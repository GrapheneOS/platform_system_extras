#include "pin_utils.h"

using namespace std;
using namespace android::base;

int write_pinlist_file(const std::string& output_file, const std::vector<VmaRange>& vmas_to_pin) {
    int pinlist_fd = open(output_file.c_str(), O_RDWR | O_CREAT, "w");
    if (pinlist_fd == -1) {
        return 1;
    }
    for (int i = 0; i < vmas_to_pin.size(); ++i) {
        uint32_t vma_start_offset = vmas_to_pin[i].offset;
        uint32_t vma_length = vmas_to_pin[i].length;
        // Transform to BigEndian as PinnerService requires that endianness for reading.
        uint32_t vma_start_offset_be = htobe32(vma_start_offset);
        uint32_t vma_length_be = htobe32(vma_length);
        int write_res = write(pinlist_fd, &vma_start_offset_be, sizeof(vma_start_offset_be));
        if (write_res == -1) {
            return 1;
        }
        write_res = write(pinlist_fd, &vma_length_be, sizeof(vma_length_be));
        if (write_res == -1) {
            return 1;
        }
    }
    close(pinlist_fd);
    return 0;
}

int read_pinlist_file(const std::string& pinner_file, /*out*/ std::vector<VmaRange>& pinranges) {
    int pinlist_fd = open(pinner_file.c_str(), O_RDWR | O_CREAT, "w");
    if (pinlist_fd == -1) {
        return 1;
    }

    uint32_t vma_start;
    uint32_t vma_length;
    while (read(pinlist_fd, &vma_start, sizeof(vma_start)) > 0) {
        int read_res = read(pinlist_fd, &vma_length, sizeof(vma_length));
        if (read_res == -1) {
            return 1;
        }
        vma_start = betoh32(vma_start);
        vma_length = betoh32(vma_length);

        pinranges.push_back(VmaRange(vma_start, vma_length));
    }

    close(pinlist_fd);
    return 0;
}