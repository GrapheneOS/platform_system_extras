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

#include <openssl/bn.h>
#include <sparse/sparse.h>

#undef NDEBUG

#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <limits>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>

#include "build_verity_tree_utils.h"
#include "hash_tree_builder.h"

#define FATAL(x...) { \
    fprintf(stderr, x); \
    exit(1); \
}

size_t verity_tree_blocks(uint64_t data_size, size_t block_size,
                          size_t hash_size, size_t level) {
  uint64_t level_blocks = div_round_up(data_size, block_size);
  uint64_t hashes_per_block = div_round_up(block_size, hash_size);

  do {
    level_blocks = div_round_up(level_blocks, hashes_per_block);
  } while (level--);

  CHECK_LE(level_blocks, std::numeric_limits<size_t>::max());
  return level_blocks;
}

void usage(void)
{
    printf("usage: build_verity_tree [ <options> ] -s <size> | <data> <verity>\n"
           "options:\n"
           "  -a,--salt-str=<string>       set salt to <string>\n"
           "  -A,--salt-hex=<hex digits>   set salt to <hex digits>\n"
           "  -h                           show this help\n"
           "  -s,--verity-size=<data size> print the size of the verity tree\n"
           "  -v,                          enable verbose logging\n"
           "  -S                           treat <data image> as a sparse file\n"
        );
}

int main(int argc, char **argv)
{
  constexpr size_t kBlockSize = 4096;

  char* data_filename;
  char* verity_filename;
  std::vector<unsigned char> salt;
  bool sparse = false;
  uint64_t calculate_size = 0;
  bool verbose = false;

  while (1) {
    const static struct option long_options[] = {
        {"salt-str", required_argument, 0, 'a'},
        {"salt-hex", required_argument, 0, 'A'},
        {"help", no_argument, 0, 'h'},
        {"sparse", no_argument, 0, 'S'},
        {"verity-size", required_argument, 0, 's'},
        {"verbose", no_argument, 0, 'v'},
        {NULL, 0, 0, 0}};
    int c = getopt_long(argc, argv, "a:A:hSs:v", long_options, NULL);
    if (c < 0) {
      break;
    }

    switch (c) {
      case 'a':
        salt.clear();
        salt.insert(salt.end(), optarg, &optarg[strlen(optarg)]);
        break;
      case 'A': {
        BIGNUM* bn = NULL;
        if (!BN_hex2bn(&bn, optarg)) {
          FATAL("failed to convert salt from hex\n");
        }
        size_t salt_size = BN_num_bytes(bn);
        salt.resize(salt_size);
        if (BN_bn2bin(bn, salt.data()) != salt_size) {
          FATAL("failed to convert salt to bytes\n");
        }
      } break;
      case 'h':
        usage();
        return 1;
      case 'S':
        sparse = true;
        break;
      case 's': {
        char* endptr;
        errno = 0;
        unsigned long long int inSize = strtoull(optarg, &endptr, 0);
        if (optarg[0] == '\0' || *endptr != '\0' ||
            (errno == ERANGE && inSize == ULLONG_MAX)) {
          FATAL("invalid value of verity-size\n");
        }
        if (inSize > UINT64_MAX) {
          FATAL("invalid value of verity-size\n");
        }
        calculate_size = (uint64_t)inSize;
      } break;
      case 'v':
        verbose = true;
        break;
      case '?':
        usage();
        return 1;
      default:
        abort();
    }
  }

  argc -= optind;
  argv += optind;

  HashTreeBuilder builder(kBlockSize);

  if (salt.empty()) {
    salt.resize(builder.hash_size());

    int random_fd = open("/dev/urandom", O_RDONLY);
    if (random_fd < 0) {
      FATAL("failed to open /dev/urandom\n");
    }

    ssize_t ret = read(random_fd, salt.data(), salt.size());
    if (ret != static_cast<ssize_t>(salt.size())) {
      FATAL("failed to read %zu bytes from /dev/urandom: %zd %d\n", salt.size(),
            ret, errno);
    }
    close(random_fd);
  }

  // TODO(xunchang) move the size calculation to HashTreeBuilder.
  if (calculate_size) {
    if (argc != 0) {
      usage();
      return 1;
    }
    size_t verity_blocks = 0;
    size_t level_blocks;
    size_t levels = 0;
    do {
      level_blocks = verity_tree_blocks(calculate_size, kBlockSize,
                                        builder.hash_size(), levels);
      levels++;
      verity_blocks += level_blocks;
    } while (level_blocks > 1);

    printf("%" PRIu64 "\n", (uint64_t)verity_blocks * kBlockSize);
    return 0;
  }

  if (argc != 2) {
    usage();
    return 1;
  }

  data_filename = argv[0];
  verity_filename = argv[1];

  android::base::unique_fd data_fd(open(data_filename, O_RDONLY));
  if (data_fd == -1) {
    FATAL("failed to open %s\n", data_filename);
  }

  struct sparse_file* file;
  if (sparse) {
    file = sparse_file_import(data_fd, false, false);
  } else {
    file = sparse_file_import_auto(data_fd, false, verbose);
  }

  if (!file) {
    FATAL("failed to read file %s\n", data_filename);
  }

  int64_t len = sparse_file_len(file, false, false);
  if (len % kBlockSize != 0) {
    FATAL("file size %" PRIu64 " is not a multiple of %zu bytes\n", len,
          kBlockSize);
  }

  // Initialize the builder to compute the hash tree.
  if (!builder.Initialize(len, salt)) {
    LOG(ERROR) << "Failed to initialize HashTreeBuilder";
    return 1;
  }

  auto hash_callback = [](void* priv, const void* data, size_t len) {
    auto sparse_hasher = static_cast<HashTreeBuilder*>(priv);
    return sparse_hasher->Update(static_cast<const unsigned char*>(data), len)
               ? 0
               : 1;
  };
  sparse_file_callback(file, false, false, hash_callback, &builder);
  sparse_file_destroy(file);

  if (!builder.BuildHashTree()) {
    return 1;
  }

  if (!builder.WriteHashTreeToFile(verity_filename)) {
    return 1;
  }

  // Output the root hash and the salt.
  for (const auto& c : builder.root_hash()) {
    printf("%02x", c);
  }
  printf(" ");
  for (const auto& c : salt) {
    printf("%02x", c);
  }
  printf("\n");

  return 0;
}
