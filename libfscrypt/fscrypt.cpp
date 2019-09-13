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

// TODO: switch to <linux/fscrypt.h> once it's in Bionic
#ifndef FSCRYPT_POLICY_V1

// Careful: due to an API quirk this is actually 0, not 1.  We use 1 everywhere
// else, so make sure to only use this constant in the ioctl itself.
#define FSCRYPT_POLICY_V1 0
#define FSCRYPT_KEY_DESCRIPTOR_SIZE 8
struct fscrypt_policy_v1 {
    __u8 version;
    __u8 contents_encryption_mode;
    __u8 filenames_encryption_mode;
    __u8 flags;
    __u8 master_key_descriptor[FSCRYPT_KEY_DESCRIPTOR_SIZE];
};

#define FSCRYPT_POLICY_V2 2
#define FSCRYPT_KEY_IDENTIFIER_SIZE 16
struct fscrypt_policy_v2 {
    __u8 version;
    __u8 contents_encryption_mode;
    __u8 filenames_encryption_mode;
    __u8 flags;
    __u8 __reserved[4];
    __u8 master_key_identifier[FSCRYPT_KEY_IDENTIFIER_SIZE];
};

#endif /* FSCRYPT_POLICY_V1 */

/* modes not supported by upstream kernel, so not in <linux/fs.h> */
#define FS_ENCRYPTION_MODE_AES_256_HEH      126
#define FS_ENCRYPTION_MODE_PRIVATE          127

#define HEX_LOOKUP "0123456789abcdef"

#define MAX_KEY_REF_SIZE_HEX (2 * FSCRYPT_KEY_IDENTIFIER_SIZE + 1)

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

static void keyrefstring(const char* key_raw_ref, size_t key_raw_ref_length, char* hex) {
    size_t j = 0;
    for (size_t i = 0; i < key_raw_ref_length; i++) {
        hex[j++] = HEX_LOOKUP[(key_raw_ref[i] & 0xF0) >> 4];
        hex[j++] = HEX_LOOKUP[key_raw_ref[i] & 0x0F];
    }
    hex[j] = '\0';
}

static uint8_t fscrypt_get_policy_flags(int filenames_encryption_mode, int policy_version) {
    uint8_t flags = 0;

    // In the original setting of v1 policies and AES-256-CTS we used 4-byte
    // padding of filenames, so we have to retain that for compatibility.
    //
    // For everything else, use 16-byte padding.  This is more secure (it helps
    // hide the length of filenames), and it makes the inputs evenly divisible
    // into cipher blocks which is more efficient for encryption and decryption.
    if (policy_version == 1 && filenames_encryption_mode == FS_ENCRYPTION_MODE_AES_256_CTS) {
        flags |= FS_POLICY_FLAGS_PAD_4;
    } else {
        flags |= FS_POLICY_FLAGS_PAD_16;
    }

    // Use DIRECT_KEY for Adiantum, since it's much more efficient but just as
    // secure since Android doesn't reuse the same master key for multiple
    // encryption modes.
    if (filenames_encryption_mode == FS_ENCRYPTION_MODE_ADIANTUM) {
        flags |= FS_POLICY_FLAG_DIRECT_KEY;
    }

    return flags;
}

static bool fscrypt_is_encrypted(int fd) {
    fscrypt_policy_v1 policy;

    // success => encrypted with v1 policy
    // EINVAL => encrypted with v2 policy
    // ENODATA => not encrypted
    return ioctl(fd, FS_IOC_GET_ENCRYPTION_POLICY, &policy) == 0 || errno == EINVAL;
}

int fscrypt_policy_ensure(const char* directory, const char* key_raw_ref, size_t key_raw_ref_length,
                          const char* contents_encryption_mode,
                          const char* filenames_encryption_mode, int policy_version) {
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

    union {
        fscrypt_policy_v1 v1;
        fscrypt_policy_v2 v2;
    } policy;
    memset(&policy, 0, sizeof(policy));

    switch (policy_version) {
        case 1:
            if (key_raw_ref_length != FSCRYPT_KEY_DESCRIPTOR_SIZE) {
                LOG(ERROR) << "Invalid key ref length for v1 policy: " << key_raw_ref_length;
                return -1;
            }
            // Careful: FSCRYPT_POLICY_V1 is actually 0 in the API, so make sure
            // to use it here instead of a literal 1.
            policy.v1.version = FSCRYPT_POLICY_V1;
            policy.v1.contents_encryption_mode = contents_mode;
            policy.v1.filenames_encryption_mode = filenames_mode;
            policy.v1.flags = fscrypt_get_policy_flags(filenames_mode, policy_version);
            memcpy(policy.v1.master_key_descriptor, key_raw_ref, FSCRYPT_KEY_DESCRIPTOR_SIZE);
            break;
        case 2:
            if (key_raw_ref_length != FSCRYPT_KEY_IDENTIFIER_SIZE) {
                LOG(ERROR) << "Invalid key ref length for v2 policy: " << key_raw_ref_length;
                return -1;
            }
            policy.v2.version = FSCRYPT_POLICY_V2;
            policy.v2.contents_encryption_mode = contents_mode;
            policy.v2.filenames_encryption_mode = filenames_mode;
            policy.v2.flags = fscrypt_get_policy_flags(filenames_mode, policy_version);
            memcpy(policy.v2.master_key_identifier, key_raw_ref, FSCRYPT_KEY_IDENTIFIER_SIZE);
            break;
        default:
            LOG(ERROR) << "Invalid encryption policy version: " << policy_version;
            return -1;
    }

    char ref[MAX_KEY_REF_SIZE_HEX];
    keyrefstring(key_raw_ref, key_raw_ref_length, ref);

    android::base::unique_fd fd(open(directory, O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (fd == -1) {
        PLOG(ERROR) << "Failed to open directory " << directory;
        return -1;
    }

    bool already_encrypted = fscrypt_is_encrypted(fd);

    // FS_IOC_SET_ENCRYPTION_POLICY will set the policy if the directory is
    // unencrypted; otherwise it will verify that the existing policy matches.
    // Setting the policy will fail if the directory is already nonempty.
    if (ioctl(fd, FS_IOC_SET_ENCRYPTION_POLICY, &policy) != 0) {
        std::string reason;
        switch (errno) {
            case EEXIST:
                reason = "The directory already has a different encryption policy.";
                break;
            default:
                reason = strerror(errno);
                break;
        }
        LOG(ERROR) << "Failed to set encryption policy of " << directory << " to " << ref
                   << " modes " << contents_mode << "/" << filenames_mode << ": " << reason;
        if (errno == ENOTEMPTY) {
            log_ls(directory);
        }
        return -1;
    }

    if (already_encrypted) {
        LOG(INFO) << "Verified that " << directory << " has the encryption policy " << ref
                  << " modes " << contents_mode << "/" << filenames_mode;
    } else {
        LOG(INFO) << "Encryption policy of " << directory << " set to " << ref << " modes "
                  << contents_mode << "/" << filenames_mode;
    }
    return 0;
}
