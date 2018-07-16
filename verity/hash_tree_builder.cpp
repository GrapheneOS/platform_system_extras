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

#include "hash_tree_builder.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>

#include "build_verity_tree_utils.h"

HashTreeBuilder::HashTreeBuilder(size_t block_size)
    : block_size_(block_size), data_size_(0), md_(EVP_sha256()) {
  CHECK(md_ != nullptr) << "Failed to initialize md";

  hash_size_ = EVP_MD_size(md_);
  CHECK_LT(hash_size_ * 2, block_size_);
}

bool HashTreeBuilder::Initialize(int64_t expected_data_size,
                                 const std::vector<unsigned char>& salt) {
  data_size_ = expected_data_size;
  salt_ = salt;

  if (data_size_ % block_size_ != 0) {
    LOG(ERROR) << "file size " << data_size_
               << " is not a multiple of block size " << block_size_;
    return false;
  }

  // Reserve enough space for the hash of the input data.
  size_t base_level_blocks =
      verity_tree_blocks(data_size_, block_size_, hash_size_, 0);
  std::vector<unsigned char> base_level;
  base_level.reserve(base_level_blocks * block_size_);
  verity_tree_.emplace_back(std::move(base_level));

  // Save the hash of the zero block to avoid future recalculation.
  std::vector<unsigned char> zero_block(block_size_, 0);
  zero_block_hash_.resize(hash_size_);
  HashBlock(zero_block.data(), zero_block_hash_.data());

  return true;
}

bool HashTreeBuilder::HashBlock(const unsigned char* block,
                                unsigned char* out) {
  unsigned int s;
  int ret = 1;

  EVP_MD_CTX* mdctx = EVP_MD_CTX_create();
  CHECK(mdctx != nullptr);
  ret &= EVP_DigestInit_ex(mdctx, md_, nullptr);
  ret &= EVP_DigestUpdate(mdctx, salt_.data(), salt_.size());
  ret &= EVP_DigestUpdate(mdctx, block, block_size_);
  ret &= EVP_DigestFinal_ex(mdctx, out, &s);
  EVP_MD_CTX_destroy(mdctx);

  CHECK_EQ(1, ret);
  CHECK_EQ(hash_size_, s);
  return true;
}

bool HashTreeBuilder::HashBlocks(const unsigned char* data, size_t len,
                                 std::vector<unsigned char>* output_vector) {
  if (len == 0) {
    return true;
  }
  CHECK_EQ(0, len % block_size_);

  if (data == nullptr) {
    for (size_t i = 0; i < len; i += block_size_) {
      output_vector->insert(output_vector->end(), zero_block_hash_.begin(),
                            zero_block_hash_.end());
    }
    return true;
  }

  for (size_t i = 0; i < len; i += block_size_) {
    unsigned char hash_buffer[hash_size_];
    if (!HashBlock(data + i, hash_buffer)) {
      return false;
    }
    output_vector->insert(output_vector->end(), hash_buffer,
                          hash_buffer + hash_size_);
  }

  return true;
}

bool HashTreeBuilder::Update(const unsigned char* data, size_t len) {
  CHECK_GT(data_size_, 0);

  return HashBlocks(data, len, &verity_tree_[0]);
}

bool HashTreeBuilder::BuildHashTree() {
  // Expects only the base level in the verity_tree_.
  CHECK_EQ(1, verity_tree_.size());

  // Expects the base level to have the same size as the total hash size of
  // input data.
  AppendPaddings(&verity_tree_.back());
  size_t base_level_blocks =
      verity_tree_blocks(data_size_, block_size_, hash_size_, 0);
  CHECK_EQ(base_level_blocks * block_size_, verity_tree_[0].size());

  while (verity_tree_.back().size() > block_size_) {
    const auto& current_level = verity_tree_.back();
    // Computes the next level of the verity tree based on the hash of the
    // current level.
    size_t next_level_blocks =
        verity_tree_blocks(current_level.size(), block_size_, hash_size_, 0);
    std::vector<unsigned char> next_level;
    next_level.reserve(next_level_blocks * block_size_);

    HashBlocks(current_level.data(), current_level.size(), &next_level);
    AppendPaddings(&next_level);

    // Checks the size of the next level.
    CHECK_EQ(next_level_blocks * block_size_, next_level.size());
    verity_tree_.emplace_back(std::move(next_level));
  }

  CHECK_EQ(block_size_, verity_tree_.back().size());
  HashBlocks(verity_tree_.back().data(), block_size_, &root_hash_);

  return true;
}

bool HashTreeBuilder::WriteHashTreeToFile(const std::string& output) const {
  android::base::unique_fd output_fd(
      open(output.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666));
  if (output_fd == -1) {
    PLOG(ERROR) << "Failed to open output file " << output;
    return false;
  }

  return WriteHashTreeToFd(output_fd);
}

bool HashTreeBuilder::WriteHashTreeToFd(int fd) const {
  CHECK(!verity_tree_.empty());

  // Reads reversely to output the verity tree top-down.
  for (size_t i = verity_tree_.size(); i > 0; i--) {
    const auto& level_blocks = verity_tree_[i - 1];
    if (!android::base::WriteFully(fd, level_blocks.data(),
                                   level_blocks.size())) {
      PLOG(ERROR) << "Failed to write the hash tree level " << i;
      return false;
    }
  }

  return true;
}

void HashTreeBuilder::AppendPaddings(std::vector<unsigned char>* data) {
  size_t remainder = data->size() % block_size_;
  if (remainder != 0) {
    data->resize(data->size() + block_size_ - remainder, 0);
  }
}
