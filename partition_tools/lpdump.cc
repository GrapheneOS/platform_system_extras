/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <android-base/strings.h>
#include <android-base/parseint.h>
#ifdef __ANDROID__
#include <fs_mgr.h>
#endif
#include <liblp/liblp.h>

using namespace android;
using namespace android::fs_mgr;

static int usage(int /* argc */, char* argv[]) {
    fprintf(stderr,
            "%s - command-line tool for dumping Android Logical Partition images.\n"
            "\n"
            "Usage:\n"
            "  %s [-s <SLOT#>|--slot=<SLOT#>] [FILE|DEVICE]\n"
            "\n"
            "Options:\n"
            "  -s, --slot=N     Slot number or suffix.\n",
            argv[0], argv[0]);
    return EX_USAGE;
}

static std::string BuildFlagString(const std::vector<std::string>& strings) {
    return strings.empty() ? "none" : android::base::Join(strings, ",");
}

static std::string BuildAttributeString(uint32_t attrs) {
    std::vector<std::string> strings;
    if (attrs & LP_PARTITION_ATTR_READONLY) strings.emplace_back("readonly");
    if (attrs & LP_PARTITION_ATTR_SLOT_SUFFIXED) strings.emplace_back("slot-suffixed");
    return BuildFlagString(strings);
}

static std::string BuildGroupFlagString(uint32_t flags) {
    std::vector<std::string> strings;
    if (flags & LP_GROUP_SLOT_SUFFIXED) strings.emplace_back("slot-suffixed");
    return BuildFlagString(strings);
}

static std::string BuildBlockDeviceFlagString(uint32_t flags) {
    std::vector<std::string> strings;
    if (flags & LP_BLOCK_DEVICE_SLOT_SUFFIXED) strings.emplace_back("slot-suffixed");
    return BuildFlagString(strings);
}

static bool IsBlockDevice(const char* file) {
    struct stat s;
    return !stat(file, &s) && S_ISBLK(s.st_mode);
}

class FileOrBlockDeviceOpener final : public PartitionOpener {
public:
    android::base::unique_fd Open(const std::string& path, int flags) const override {
        // Try a local file first.
        android::base::unique_fd fd(open(path.c_str(), flags));
        if (fd >= 0) {
            return fd;
        }
        return PartitionOpener::Open(path, flags);
    }
};

int main(int argc, char* argv[]) {
    struct option options[] = {
        { "slot", required_argument, nullptr, 's' },
        { "help", no_argument, nullptr, 'h' },
        { nullptr, 0, nullptr, 0 },
    };

    int rv;
    int index;
    uint32_t slot = 0;
    while ((rv = getopt_long_only(argc, argv, "s:h", options, &index)) != -1) {
        switch (rv) {
            case 'h':
                return usage(argc, argv);
            case 's':
                if (!android::base::ParseUint(optarg, &slot)) {
                    slot = SlotNumberForSlotSuffix(optarg);
                }
                break;
        }
    }

    std::unique_ptr<LpMetadata> pt;
    if (optind < argc) {
        const char* file = argv[optind++];

        FileOrBlockDeviceOpener opener;
        pt = ReadMetadata(opener, file, slot);
        if (!pt && !IsBlockDevice(file)) {
            pt = ReadFromImageFile(file);
        }
    } else {
#ifdef __ANDROID__
        auto slot_number = SlotNumberForSlotSuffix(fs_mgr_get_slot_suffix());
        pt = ReadMetadata(fs_mgr_get_super_partition_name(), slot_number);
#else
        return usage(argc, argv);
#endif
    }
    if (!pt) {
        fprintf(stderr, "Failed to read metadata.\n");
        return EX_NOINPUT;
    }

    printf("Metadata version: %u.%u\n", pt->header.major_version, pt->header.minor_version);
    printf("Metadata size: %u bytes\n", pt->header.header_size + pt->header.tables_size);
    printf("Metadata max size: %u bytes\n", pt->geometry.metadata_max_size);
    printf("Metadata slot count: %u\n", pt->geometry.metadata_slot_count);
    printf("Partition table:\n");
    printf("------------------------\n");

    for (const auto& partition : pt->partitions) {
        std::string name = GetPartitionName(partition);
        std::string group_name = GetPartitionGroupName(pt->groups[partition.group_index]);
        printf("  Name: %s\n", name.c_str());
        printf("  Group: %s\n", group_name.c_str());
        printf("  Attributes: %s\n", BuildAttributeString(partition.attributes).c_str());
        printf("  Extents:\n");
        uint64_t first_sector = 0;
        for (size_t i = 0; i < partition.num_extents; i++) {
            const LpMetadataExtent& extent = pt->extents[partition.first_extent_index + i];
            printf("    %" PRIu64 " .. %" PRIu64 " ", first_sector,
                    (first_sector + extent.num_sectors - 1));
            first_sector += extent.num_sectors;
            if (extent.target_type == LP_TARGET_TYPE_LINEAR) {
                const auto& block_device = pt->block_devices[extent.target_source];
                std::string device_name = GetBlockDevicePartitionName(block_device);
                printf("linear %s %" PRIu64, device_name.c_str(), extent.target_data);
            } else if (extent.target_type == LP_TARGET_TYPE_ZERO) {
                printf("zero");
            }
            printf("\n");
        }
        printf("------------------------\n");
    }

    printf("Block device table:\n");
    printf("------------------------\n");
    for (const auto& block_device : pt->block_devices) {
        std::string partition_name = GetBlockDevicePartitionName(block_device);
        printf("  Partition name: %s\n", partition_name.c_str());
        printf("  First sector: %" PRIu64 "\n", block_device.first_logical_sector);
        printf("  Size: %" PRIu64 " bytes\n", block_device.size);
        printf("  Flags: %s\n", BuildBlockDeviceFlagString(block_device.flags).c_str());
        printf("------------------------\n");
    }

    printf("Group table:\n");
    printf("------------------------\n");
    for (const auto& group : pt->groups) {
        std::string group_name = GetPartitionGroupName(group);
        printf("  Name: %s\n", group_name.c_str());
        printf("  Maximum size: %" PRIu64 "\n", group.maximum_size);
        printf("  Flags: %s\n", BuildGroupFlagString(group.flags).c_str());
        printf("------------------------\n");
    }

    return EX_OK;
}
