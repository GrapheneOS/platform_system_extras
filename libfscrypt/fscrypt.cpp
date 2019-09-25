/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "fscrypt/fscrypt.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <asm/ioctl.h>
#include <cutils/properties.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <logwrap/logwrap.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utils/misc.h>

#include <array>

#define FS_KEY_DESCRIPTOR_SIZE_HEX (2 * FS_KEY_DESCRIPTOR_SIZE + 1)

/* modes not supported by upstream kernel, so not in <linux/fs.h> */
#define FS_ENCRYPTION_MODE_AES_256_HEH      126
#define FS_ENCRYPTION_MODE_PRIVATE          127

#define HEX_LOOKUP "0123456789abcdef"

bool fscrypt_is_native() {
    char value[PROPERTY_VALUE_MAX];
    property_get("ro.crypto.type", value, "none");
    return !strcmp(value, "file");
}

static void log_ls(const char* dirname) {
    std::array<const char*, 3> argv = {"ls", "-laZ", dirname};
    int status = 0;
    auto res =
        logwrap_fork_execvp(argv.size(), argv.data(), &status, false, LOG_ALOG, false, nullptr);
    if (res != 0) {
        PLOG(ERROR) << argv[0] << " " << argv[1] << " " << argv[2] << "failed";
        return;
    }
    if (!WIFEXITED(status)) {
        LOG(ERROR) << argv[0] << " " << argv[1] << " " << argv[2]
                   << " did not exit normally, status: " << status;
        return;
    }
    if (WEXITSTATUS(status) != 0) {
        LOG(ERROR) << argv[0] << " " << argv[1] << " " << argv[2]
                   << " returned failure: " << WEXITSTATUS(status);
        return;
    }
}

static void policy_to_hex(const char* policy, char* hex) {
    for (size_t i = 0, j = 0; i < FS_KEY_DESCRIPTOR_SIZE; i++) {
        hex[j++] = HEX_LOOKUP[(policy[i] & 0xF0) >> 4];
        hex[j++] = HEX_LOOKUP[policy[i] & 0x0F];
    }
    hex[FS_KEY_DESCRIPTOR_SIZE_HEX - 1] = '\0';
}

static uint8_t fscrypt_get_policy_flags(int filenames_encryption_mode) {
    if (filenames_encryption_mode == FS_ENCRYPTION_MODE_AES_256_CTS) {
        // Use legacy padding with our original filenames encryption mode.
        return FS_POLICY_FLAGS_PAD_4;
    } else if (filenames_encryption_mode == FS_ENCRYPTION_MODE_ADIANTUM) {
        // Use DIRECT_KEY for Adiantum, since it's much more efficient but just
        // as secure since Android doesn't reuse the same master key for
        // multiple encryption modes
        return (FS_POLICY_FLAGS_PAD_16 | FS_POLICY_FLAG_DIRECT_KEY);
    }
    // With a new mode we can use the better padding flag without breaking existing devices: pad
    // filenames with zeroes to the next 16-byte boundary.  This is more secure (helps hide the
    // length of filenames) and makes the inputs evenly divisible into blocks which is more
    // efficient for encryption and decryption.
    return FS_POLICY_FLAGS_PAD_16;
}

static bool fscrypt_is_encrypted(int fd) {
    fscrypt_policy fp;
    return ioctl(fd, FS_IOC_GET_ENCRYPTION_POLICY, &fp) == 0;
}

int fscrypt_policy_ensure(const char *directory, const char *policy,
                          size_t policy_length,
                          const char *contents_encryption_mode,
                          const char *filenames_encryption_mode) {
    int contents_mode = 0;
    int filenames_mode = 0;

    if (!strcmp(contents_encryption_mode, "software") ||
        !strcmp(contents_encryption_mode, "aes-256-xts")) {
        contents_mode = FS_ENCRYPTION_MODE_AES_256_XTS;
    } else if (!strcmp(contents_encryption_mode, "adiantum")) {
        contents_mode = FS_ENCRYPTION_MODE_ADIANTUM;
    } else if (!strcmp(contents_encryption_mode, "ice")) {
        contents_mode = FS_ENCRYPTION_MODE_PRIVATE;
    } else {
        LOG(ERROR) << "Invalid file contents encryption mode: "
                   << contents_encryption_mode;
        return -1;
    }

    if (!strcmp(filenames_encryption_mode, "aes-256-cts")) {
        filenames_mode = FS_ENCRYPTION_MODE_AES_256_CTS;
    } else if (!strcmp(filenames_encryption_mode, "aes-256-heh")) {
        filenames_mode = FS_ENCRYPTION_MODE_AES_256_HEH;
    } else if (!strcmp(filenames_encryption_mode, "adiantum")) {
        filenames_mode = FS_ENCRYPTION_MODE_ADIANTUM;
    } else {
        LOG(ERROR) << "Invalid file names encryption mode: "
                   << filenames_encryption_mode;
        return -1;
    }

    if (policy_length != FS_KEY_DESCRIPTOR_SIZE) {
        LOG(ERROR) << "Policy wrong length: " << policy_length;
        return -1;
    }
    char policy_hex[FS_KEY_DESCRIPTOR_SIZE_HEX];
    policy_to_hex(policy, policy_hex);

    fscrypt_policy fp;
    fp.version = 0;
    fp.contents_encryption_mode = contents_mode;
    fp.filenames_encryption_mode = filenames_mode;
    fp.flags = fscrypt_get_policy_flags(filenames_mode);
    memcpy(fp.master_key_descriptor, policy, FS_KEY_DESCRIPTOR_SIZE);

    android::base::unique_fd fd(open(directory, O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (fd == -1) {
        PLOG(ERROR) << "Failed to open directory " << directory;
        return -1;
    }

    bool already_encrypted = fscrypt_is_encrypted(fd);

    // FS_IOC_SET_ENCRYPTION_POLICY will set the policy if the directory is
    // unencrypted; otherwise it will verify that the existing policy matches.
    // Setting the policy will fail if the directory is already nonempty.
    if (ioctl(fd, FS_IOC_SET_ENCRYPTION_POLICY, &fp) != 0) {
        std::string reason;
        switch (errno) {
            case EEXIST:
                reason = "The directory already has a different encryption policy.";
                break;
            default:
                reason = strerror(errno);
                break;
        }
        LOG(ERROR) << "Failed to set encryption policy of " << directory << " to " << policy_hex
                   << " modes " << contents_mode << "/" << filenames_mode << ": " << reason;
        if (errno == ENOTEMPTY) {
            log_ls(directory);
        }
        return -1;
    }

    if (already_encrypted) {
        LOG(INFO) << "Verified that " << directory << " has the encryption policy " << policy_hex
                  << " modes " << contents_mode << "/" << filenames_mode;
    } else {
        LOG(INFO) << "Encryption policy of " << directory << " set to " << policy_hex << " modes "
                  << contents_mode << "/" << filenames_mode;
    }
    return 0;
}
