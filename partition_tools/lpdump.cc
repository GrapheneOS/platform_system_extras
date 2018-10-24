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

#include <android-base/parseint.h>
#include <liblp/liblp.h>

using namespace android;
using namespace android::fs_mgr;

static int usage(int /* argc */, char* argv[]) {
    fprintf(stderr,
            "%s - command-line tool for dumping Android Logical Partition images.\n"
            "\n"
            "Usage:\n"
            "  %s [-s,--slot] file-or-device\n"
            "\n"
            "Options:\n"
            "  -s, --slot=N     Slot number or suffix.\n",
            argv[0], argv[0]);
    return EX_USAGE;
}

static std::string BuildAttributeString(uint32_t attrs) {
    return (attrs & LP_PARTITION_ATTR_READONLY) ? "readonly" : "none";
}

static bool IsBlockDevice(const char* file) {
    struct stat s;
    return !stat(file, &s) && S_ISBLK(s.st_mode);
}

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

    if (optind >= argc) {
        return usage(argc, argv);
    }
    const char* file = argv[optind++];

    std::unique_ptr<LpMetadata> pt;
    if (IsBlockDevice(file)) {
        pt = ReadMetadata(file, slot);
    } else {
        pt = ReadFromImageFile(file);
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
        printf("------------------------\n");
    }

    printf("Group table:\n");
    printf("------------------------\n");
    for (const auto& group : pt->groups) {
        std::string group_name = GetPartitionGroupName(group);
        printf("  Name: %s\n", group_name.c_str());
        printf("  Maximum size: %" PRIx64 "\n", group.maximum_size);
        printf("------------------------\n");
    }

    return EX_OK;
}
