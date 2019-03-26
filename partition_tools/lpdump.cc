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
#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>
#include <unistd.h>

#include <iostream>
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

static int usage(int /* argc */, char* argv[], std::ostream& cerr) {
    cerr << argv[0]
         << " - command-line tool for dumping Android Logical Partition images.\n"
            "\n"
            "Usage:\n"
            "  "
         << argv[0]
         << " [-s <SLOT#>|--slot=<SLOT#>] [FILE|DEVICE]\n"
            "\n"
            "Options:\n"
            "  -s, --slot=N     Slot number or suffix.\n";
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

int LpdumpMain(int argc, char* argv[], std::ostream& cout, std::ostream& cerr) {
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
                return usage(argc, argv, cerr);
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
        return usage(argc, argv, cerr);
#endif
    }
    if (!pt) {
        cerr << "Failed to read metadata.\n";
        return EX_NOINPUT;
    }

    cout << "Metadata version: " << pt->header.major_version << "." << pt->header.minor_version
         << "\n";
    cout << "Metadata size: " << (pt->header.header_size + pt->header.tables_size) << " bytes\n";
    cout << "Metadata max size: " << pt->geometry.metadata_max_size << " bytes\n";
    cout << "Metadata slot count: " << pt->geometry.metadata_slot_count << "\n";
    cout << "Partition table:\n";
    cout << "------------------------\n";

    for (const auto& partition : pt->partitions) {
        std::string name = GetPartitionName(partition);
        std::string group_name = GetPartitionGroupName(pt->groups[partition.group_index]);
        cout << "  Name: " << name << "\n";
        cout << "  Group: " << group_name << "\n";
        cout << "  Attributes: " << BuildAttributeString(partition.attributes) << "\n";
        cout << "  Extents:\n";
        uint64_t first_sector = 0;
        for (size_t i = 0; i < partition.num_extents; i++) {
            const LpMetadataExtent& extent = pt->extents[partition.first_extent_index + i];
            cout << "    " << first_sector << " .. " << (first_sector + extent.num_sectors - 1)
                 << " ";
            first_sector += extent.num_sectors;
            if (extent.target_type == LP_TARGET_TYPE_LINEAR) {
                const auto& block_device = pt->block_devices[extent.target_source];
                std::string device_name = GetBlockDevicePartitionName(block_device);
                cout << "linear " << device_name.c_str() << " " << extent.target_data;
            } else if (extent.target_type == LP_TARGET_TYPE_ZERO) {
                cout << "zero";
            }
            cout << "\n";
        }
        cout << "------------------------\n";
    }

    cout << "Block device table:\n";
    cout << "------------------------\n";
    for (const auto& block_device : pt->block_devices) {
        std::string partition_name = GetBlockDevicePartitionName(block_device);
        cout << "  Partition name: " << partition_name << "\n";
        cout << "  First sector: " << block_device.first_logical_sector << "\n";
        cout << "  Size: " << block_device.size << " bytes\n";
        cout << "  Flags: " << BuildBlockDeviceFlagString(block_device.flags) << "\n";
        cout << "------------------------\n";
    }

    cout << "Group table:\n";
    cout << "------------------------\n";
    for (const auto& group : pt->groups) {
        std::string group_name = GetPartitionGroupName(group);
        cout << "  Name: " << group_name << "\n";
        cout << "  Maximum size: " << group.maximum_size << " bytes\n";
        cout << "  Flags: " << BuildGroupFlagString(group.flags) << "\n";
        cout << "------------------------\n";
    }

    return EX_OK;
}

int main(int argc, char* argv[]) {
    return LpdumpMain(argc, argv, std::cout, std::cerr);
}
