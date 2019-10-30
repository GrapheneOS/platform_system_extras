/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <linux/fs.h>

#include <fscrypt/fscrypt.h>

#include <gtest/gtest.h>

using namespace android::fscrypt;

/* modes not supported by upstream kernel, so not in <linux/fs.h> */
#define FS_ENCRYPTION_MODE_AES_256_HEH 126
#define FS_ENCRYPTION_MODE_PRIVATE 127

TEST(fscrypt, ParseOptions) {
    EncryptionOptions options;
    std::string options_string;

    EXPECT_FALSE(ParseOptions("", &options));
    EXPECT_FALSE(ParseOptions("blah", &options));

    EXPECT_TRUE(ParseOptions("software", &options));
    EXPECT_EQ(1, options.version);
    EXPECT_EQ(FS_ENCRYPTION_MODE_AES_256_XTS, options.contents_mode);
    EXPECT_EQ(FS_ENCRYPTION_MODE_AES_256_CTS, options.filenames_mode);
    EXPECT_EQ(FS_POLICY_FLAGS_PAD_4, options.flags);
    EXPECT_TRUE(OptionsToString(options, &options_string));
    EXPECT_EQ("aes-256-xts:aes-256-cts:v1", options_string);

    EXPECT_TRUE(ParseOptions("aes-256-xts", &options));
    EXPECT_EQ(1, options.version);
    EXPECT_EQ(FS_ENCRYPTION_MODE_AES_256_XTS, options.contents_mode);
    EXPECT_EQ(FS_ENCRYPTION_MODE_AES_256_CTS, options.filenames_mode);
    EXPECT_EQ(FS_POLICY_FLAGS_PAD_4, options.flags);
    EXPECT_TRUE(OptionsToString(options, &options_string));
    EXPECT_EQ("aes-256-xts:aes-256-cts:v1", options_string);

    EXPECT_TRUE(ParseOptions("adiantum", &options));
    EXPECT_EQ(1, options.version);
    EXPECT_EQ(FS_ENCRYPTION_MODE_ADIANTUM, options.contents_mode);
    EXPECT_EQ(FS_ENCRYPTION_MODE_ADIANTUM, options.filenames_mode);
    EXPECT_EQ(FS_POLICY_FLAGS_PAD_16 | FS_POLICY_FLAG_DIRECT_KEY, options.flags);
    EXPECT_TRUE(OptionsToString(options, &options_string));
    EXPECT_EQ("adiantum:adiantum:v1", options_string);

    EXPECT_TRUE(ParseOptions("adiantum:aes-256-heh", &options));
    EXPECT_EQ(1, options.version);
    EXPECT_EQ(FS_ENCRYPTION_MODE_ADIANTUM, options.contents_mode);
    EXPECT_EQ(FS_ENCRYPTION_MODE_AES_256_HEH, options.filenames_mode);
    EXPECT_EQ(FS_POLICY_FLAGS_PAD_16, options.flags);
    EXPECT_TRUE(OptionsToString(options, &options_string));
    EXPECT_EQ("adiantum:aes-256-heh:v1", options_string);

    EXPECT_TRUE(ParseOptions("ice", &options));
    EXPECT_EQ(1, options.version);
    EXPECT_EQ(FS_ENCRYPTION_MODE_PRIVATE, options.contents_mode);
    EXPECT_EQ(FS_ENCRYPTION_MODE_AES_256_CTS, options.filenames_mode);
    EXPECT_EQ(FS_POLICY_FLAGS_PAD_4, options.flags);
    EXPECT_TRUE(OptionsToString(options, &options_string));
    EXPECT_EQ("ice:aes-256-cts:v1", options_string);

    EXPECT_FALSE(ParseOptions("ice:blah", &options));

    EXPECT_TRUE(ParseOptions("ice:aes-256-cts", &options));
    EXPECT_EQ(1, options.version);
    EXPECT_EQ(FS_ENCRYPTION_MODE_PRIVATE, options.contents_mode);
    EXPECT_EQ(FS_ENCRYPTION_MODE_AES_256_CTS, options.filenames_mode);
    EXPECT_EQ(FS_POLICY_FLAGS_PAD_4, options.flags);
    EXPECT_TRUE(OptionsToString(options, &options_string));
    EXPECT_EQ("ice:aes-256-cts:v1", options_string);

    EXPECT_TRUE(ParseOptions("ice:aes-256-heh", &options));
    EXPECT_EQ(1, options.version);
    EXPECT_EQ(FS_ENCRYPTION_MODE_PRIVATE, options.contents_mode);
    EXPECT_EQ(FS_ENCRYPTION_MODE_AES_256_HEH, options.filenames_mode);
    EXPECT_EQ(FS_POLICY_FLAGS_PAD_16, options.flags);
    EXPECT_TRUE(OptionsToString(options, &options_string));
    EXPECT_EQ("ice:aes-256-heh:v1", options_string);

    EXPECT_TRUE(ParseOptions("ice:adiantum", &options));
    EXPECT_EQ(1, options.version);
    EXPECT_EQ(FS_ENCRYPTION_MODE_PRIVATE, options.contents_mode);
    EXPECT_EQ(FS_ENCRYPTION_MODE_ADIANTUM, options.filenames_mode);
    EXPECT_EQ(FS_POLICY_FLAGS_PAD_16 | FS_POLICY_FLAG_DIRECT_KEY, options.flags);
    EXPECT_TRUE(OptionsToString(options, &options_string));
    EXPECT_EQ("ice:adiantum:v1", options_string);

    EXPECT_TRUE(ParseOptions("aes-256-xts:aes-256-cts", &options));
    EXPECT_EQ(1, options.version);
    EXPECT_EQ(FS_ENCRYPTION_MODE_AES_256_XTS, options.contents_mode);
    EXPECT_EQ(FS_ENCRYPTION_MODE_AES_256_CTS, options.filenames_mode);
    EXPECT_EQ(FS_POLICY_FLAGS_PAD_4, options.flags);
    EXPECT_TRUE(OptionsToString(options, &options_string));
    EXPECT_EQ("aes-256-xts:aes-256-cts:v1", options_string);

    EXPECT_TRUE(ParseOptions("aes-256-xts:aes-256-cts:v1", &options));
    EXPECT_EQ(1, options.version);
    EXPECT_EQ(FS_ENCRYPTION_MODE_AES_256_XTS, options.contents_mode);
    EXPECT_EQ(FS_ENCRYPTION_MODE_AES_256_CTS, options.filenames_mode);
    EXPECT_EQ(FS_POLICY_FLAGS_PAD_4, options.flags);
    EXPECT_TRUE(OptionsToString(options, &options_string));
    EXPECT_EQ("aes-256-xts:aes-256-cts:v1", options_string);

    EXPECT_TRUE(ParseOptions("aes-256-xts:aes-256-cts:v2", &options));
    EXPECT_EQ(2, options.version);
    EXPECT_EQ(FS_ENCRYPTION_MODE_AES_256_XTS, options.contents_mode);
    EXPECT_EQ(FS_ENCRYPTION_MODE_AES_256_CTS, options.filenames_mode);
    EXPECT_EQ(FS_POLICY_FLAGS_PAD_16, options.flags);
    EXPECT_TRUE(OptionsToString(options, &options_string));
    EXPECT_EQ("aes-256-xts:aes-256-cts:v2", options_string);

    EXPECT_TRUE(ParseOptions("aes-256-xts:aes-256-cts:v2+inlinecrypt_optimized", &options));
    EXPECT_EQ(2, options.version);
    EXPECT_EQ(FS_ENCRYPTION_MODE_AES_256_XTS, options.contents_mode);
    EXPECT_EQ(FS_ENCRYPTION_MODE_AES_256_CTS, options.filenames_mode);
    EXPECT_EQ(FS_POLICY_FLAGS_PAD_16 | FSCRYPT_POLICY_FLAG_IV_INO_LBLK_64, options.flags);
    EXPECT_TRUE(OptionsToString(options, &options_string));
    EXPECT_EQ("aes-256-xts:aes-256-cts:v2+inlinecrypt_optimized", options_string);

    EXPECT_FALSE(ParseOptions("aes-256-xts:aes-256-cts:v2:", &options));
    EXPECT_FALSE(ParseOptions("aes-256-xts:aes-256-cts:v2:foo", &options));
    EXPECT_FALSE(ParseOptions("aes-256-xts:aes-256-cts:blah", &options));
    EXPECT_FALSE(ParseOptions("aes-256-xts:aes-256-cts:vblah", &options));
}
