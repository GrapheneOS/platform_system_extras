/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <optional>
#include <sstream>

#include <sysexits.h>
#include <android/hardware/boot/1.1/IBootControl.h>

using android::sp;

using android::hardware::hidl_string;
using android::hardware::Return;

using android::hardware::boot::V1_0::BoolResult;
using android::hardware::boot::V1_0::CommandResult;
using android::hardware::boot::V1_0::Slot;
using android::hardware::boot::V1_1::IBootControl;
using android::hardware::boot::V1_1::MergeStatus;

static void usage(FILE* where, int /* argc */, char* argv[])
{
    fprintf(where,
            "%s - command-line wrapper for the boot HAL.\n"
            "\n"
            "Usage:\n"
            "  %s COMMAND\n"
            "\n"
            "Commands:\n"
            "  hal-info                       - Show info about boot_control HAL used.\n"
            "  get-number-slots               - Prints number of slots.\n"
            "  get-current-slot               - Prints currently running SLOT.\n"
            "  mark-boot-successful           - Mark current slot as GOOD.\n"
            "  set-active-boot-slot SLOT      - On next boot, load and execute SLOT.\n"
            "  set-slot-as-unbootable SLOT    - Mark SLOT as invalid.\n"
            "  is-slot-bootable SLOT          - Returns 0 only if SLOT is bootable.\n"
            "  is-slot-marked-successful SLOT - Returns 0 only if SLOT is marked GOOD.\n"
            "  get-suffix SLOT                - Prints suffix for SLOT.\n"
            "  set-snapshot-merge-status STAT - Sets whether a snapshot-merge of any dynamic\n"
            "                                   partition is in progress. Valid STAT values\n"
            "                                   are: none, unknown, snapshotted, merging,\n"
            "                                   or cancelled.\n"
            "  get-snapshot-merge-status      - Prints the current snapshot-merge status.\n"
            "\n"
            "SLOT parameter is the zero-based slot-number.\n",
            argv[0], argv[0]);
}

static int do_hal_info(const sp<IBootControl> module) {
    module->interfaceDescriptor([&](const auto& descriptor) {
        fprintf(stdout,
                "HAL Version: %s\n",
                descriptor.c_str());
    });
    return EX_OK;
}

static int do_get_number_slots(sp<IBootControl> module)
{
    uint32_t numSlots = module->getNumberSlots();
    fprintf(stdout, "%u\n", numSlots);
    return EX_OK;
}

static int do_get_current_slot(sp<IBootControl> module)
{
    Slot curSlot = module->getCurrentSlot();
    fprintf(stdout, "%u\n", curSlot);
    return EX_OK;
}

static std::function<void(CommandResult)> generate_callback(CommandResult *crp) {
    return [=](CommandResult cr){
        *crp = cr;
    };
}

static int handle_return(const Return<void> &ret, CommandResult cr, const char* errStr) {
    if (!ret.isOk()) {
        fprintf(stderr, errStr, ret.description().c_str());
        return EX_SOFTWARE;
    } else if (!cr.success) {
        fprintf(stderr, errStr, cr.errMsg.c_str());
        return EX_SOFTWARE;
    }
    return EX_OK;
}

static int do_mark_boot_successful(sp<IBootControl> module)
{
    CommandResult cr;
    Return<void> ret = module->markBootSuccessful(generate_callback(&cr));
    return handle_return(ret, cr, "Error marking as having booted successfully: %s\n");
}

static int do_set_active_boot_slot(sp<IBootControl> module,
                                   Slot slot_number)
{
    CommandResult cr;
    Return<void> ret = module->setActiveBootSlot(slot_number, generate_callback(&cr));
    return handle_return(ret, cr, "Error setting active boot slot: %s\n");
}

static int do_set_slot_as_unbootable(sp<IBootControl> module,
                                     Slot slot_number)
{
    CommandResult cr;
    Return<void> ret = module->setSlotAsUnbootable(slot_number, generate_callback(&cr));
    return handle_return(ret, cr, "Error setting slot as unbootable: %s\n");
}

static int handle_return(const Return<BoolResult> &ret, const char* errStr) {
    if (!ret.isOk()) {
        fprintf(stderr, errStr, ret.description().c_str());
        return EX_SOFTWARE;
    } else if (ret == BoolResult::INVALID_SLOT) {
        fprintf(stderr, errStr, "Invalid slot");
        return EX_SOFTWARE;
    } else if (ret == BoolResult::TRUE) {
        return EX_OK;
    }
    return EX_SOFTWARE;
}

static int do_is_slot_bootable(sp<IBootControl> module, Slot slot_number)
{
    Return<BoolResult> ret = module->isSlotBootable(slot_number);
    return handle_return(ret, "Error calling isSlotBootable(): %s\n");
}

static int do_is_slot_marked_successful(sp<IBootControl> module,
                                        Slot slot_number)
{
    Return<BoolResult> ret = module->isSlotMarkedSuccessful(slot_number);
    return handle_return(ret, "Error calling isSlotMarkedSuccessful(): %s\n");
}

std::optional<MergeStatus> stringToMergeStatus(const std::string &status) {
    if (status == "cancelled") return MergeStatus::CANCELLED;
    if (status == "merging") return MergeStatus::MERGING;
    if (status == "none") return MergeStatus::NONE;
    if (status == "snapshotted") return MergeStatus::SNAPSHOTTED;
    if (status == "unknown") return MergeStatus::UNKNOWN;
    return {};
}

static int do_set_snapshot_merge_status(sp<IBootControl> module, int argc, char *argv[]) {
    if (argc != 3) {
        usage(stderr, argc, argv);
        exit(EX_USAGE);
        return -1;
    }

    auto status = stringToMergeStatus(argv[2]);
    if (!status.has_value()) {
        usage(stderr, argc, argv);
        exit(EX_USAGE);
        return -1;
    }

    if (!module->setSnapshotMergeStatus(status.value())) {
        return EX_SOFTWARE;
    }
    return EX_OK;
}

std::ostream& operator<<(std::ostream& os, MergeStatus state) {
    switch (state) {
        case MergeStatus::CANCELLED:
            return os << "cancelled";
        case MergeStatus::MERGING:
            return os << "merging";
        case MergeStatus::NONE:
            return os << "none";
        case MergeStatus::SNAPSHOTTED:
            return os << "snapshotted";
        case MergeStatus::UNKNOWN:
            return os << "unknown";
        default:
            return os;
    }
}

static int do_get_snapshot_merge_status(sp<IBootControl> module) {
    MergeStatus ret = module->getSnapshotMergeStatus();
    std::stringstream ss;
    ss << ret;
    fprintf(stdout, "%s\n", ss.str().c_str());
    return EX_OK;
}

static int do_get_suffix(sp<IBootControl> module, Slot slot_number) {
    std::function<void(hidl_string)> cb = [](hidl_string suffix){
        fprintf(stdout, "%s\n", suffix.c_str());
    };
    Return<void> ret = module->getSuffix(slot_number, cb);
    if (!ret.isOk()) {
        fprintf(stderr, "Error calling getSuffix(): %s\n",
                ret.description().c_str());
        return EX_SOFTWARE;
    }
    return EX_OK;
}

static uint32_t parse_slot(int pos, int argc, char *argv[])
{
    if (pos > argc - 1) {
        usage(stderr, argc, argv);
        exit(EX_USAGE);
        return -1;
    }
    errno = 0;
    uint64_t ret = strtoul(argv[pos], NULL, 10);
    if (errno != 0 || ret > UINT_MAX) {
        usage(stderr, argc, argv);
        exit(EX_USAGE);
        return -1;
    }
    return (uint32_t)ret;
}

int main(int argc, char *argv[])
{
    sp<IBootControl> module;

    if (argc < 2) {
        usage(stderr, argc, argv);
        return EX_USAGE;
    }

    module = IBootControl::getService();
    if (module == NULL) {
        fprintf(stderr, "Error getting bootctrl module.\n");
        return EX_SOFTWARE;
    }

    if (strcmp(argv[1], "hal-info") == 0) {
        return do_hal_info(module);
    } else if (strcmp(argv[1], "get-number-slots") == 0) {
        return do_get_number_slots(module);
    } else if (strcmp(argv[1], "get-current-slot") == 0) {
        return do_get_current_slot(module);
    } else if (strcmp(argv[1], "mark-boot-successful") == 0) {
        return do_mark_boot_successful(module);
    } else if (strcmp(argv[1], "set-active-boot-slot") == 0) {
        return do_set_active_boot_slot(module, parse_slot(2, argc, argv));
    } else if (strcmp(argv[1], "set-slot-as-unbootable") == 0) {
        return do_set_slot_as_unbootable(module, parse_slot(2, argc, argv));
    } else if (strcmp(argv[1], "is-slot-bootable") == 0) {
        return do_is_slot_bootable(module, parse_slot(2, argc, argv));
    } else if (strcmp(argv[1], "get-suffix") == 0) {
        return do_get_suffix(module, parse_slot(2, argc, argv));
    } else if (strcmp(argv[1], "is-slot-marked-successful") == 0) {
        return do_is_slot_marked_successful(module, parse_slot(2, argc, argv));
    } else if (strcmp(argv[1], "set-snapshot-merge-status") == 0) {
        return do_set_snapshot_merge_status(module, argc, argv);
    } else if (strcmp(argv[1], "get-snapshot-merge-status") == 0) {
        return do_get_snapshot_merge_status(module);
    } else {
        usage(stderr, argc, argv);
        return EX_USAGE;
    }

    return 0;
}
