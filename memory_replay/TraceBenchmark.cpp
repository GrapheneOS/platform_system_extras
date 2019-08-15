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

#include <err.h>
#include <inttypes.h>
#include <malloc.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include <android-base/file.h>
#include <android-base/strings.h>
#include <benchmark/benchmark.h>
#include <ziparchive/zip_archive.h>

enum AllocEnum : uint8_t {
  MALLOC = 0,   // arg2 not used
  CALLOC,       // size = item_count, arg2 = item_size
  MEMALIGN,     // arg2 = alignment
  REALLOC,      // if arg2 = 0, ptr arg is nullptr, else arg2 = old pointer index + 1
  FREE,         // size not used, arg2 not used
};

struct MallocEntry {
  MallocEntry(AllocEnum type, size_t idx, size_t size, size_t arg2)
      : type(type), idx(idx), size(size), arg2(arg2) {}
  AllocEnum type;
  size_t idx;
  size_t size;
  size_t arg2;
};

static std::string GetZipContents(const char* filename) {
  ZipArchiveHandle archive;
  if (OpenArchive(filename, &archive) != 0) {
    return "";
  }

  std::string contents;
  void* cookie;
  if (StartIteration(archive, &cookie) == 0) {
    ZipEntry entry;
    std::string name;
    if (Next(cookie, &entry, &name) == 0) {
      contents.resize(entry.uncompressed_length);
      if (ExtractToMemory(archive, &entry, reinterpret_cast<uint8_t*>(contents.data()),
                          entry.uncompressed_length) != 0) {
        contents = "";
      }
    }
  }

  CloseArchive(archive);
  return contents;
}

static size_t GetIndex(std::stack<size_t>& indices, size_t* max_index) {
  if (indices.empty()) {
    return (*max_index)++;
  }
  size_t index = indices.top();
  indices.pop();
  return index;
}

static std::vector<MallocEntry>* GetTraceData(const char* filename, size_t* max_ptrs) {
  // Only keep last trace encountered cached.
  static std::string cached_filename;
  static std::vector<MallocEntry> cached_entries;
  static size_t cached_max_ptrs;

  if (cached_filename == filename) {
    *max_ptrs = cached_max_ptrs;
    return &cached_entries;
  }

  cached_entries.clear();
  cached_max_ptrs = 0;
  cached_filename = filename;

  std::string content(GetZipContents(filename));
  if (content.empty()) {
    errx(1, "Internal Error: Empty zip file %s", filename);
  }
  std::vector<std::string> lines(android::base::Split(content, "\n"));

  *max_ptrs = 0;
  std::stack<size_t> free_indices;
  std::unordered_map<uintptr_t, size_t> indices;
  std::vector<MallocEntry>* entries = &cached_entries;
  for (const std::string& line : lines) {
    if (line.empty()) {
      continue;
    }
    pid_t tid;
    int line_pos = 0;
    char name[128];
    uintptr_t pointer;
    // All lines have this format:
    //   TID: ALLOCATION_TYPE POINTER
    // where
    //   TID is the thread id of the thread doing the operation.
    //   ALLOCATION_TYPE is one of malloc, calloc, memalign, realloc, free, thread_done
    //   POINTER is the hex value of the actual pointer
    if (sscanf(line.c_str(), "%d: %127s %" SCNxPTR " %n", &tid, name, &pointer, &line_pos) != 3) {
      errx(1, "Internal Error: Failed to process %s", line.c_str());
    }
    const char* line_end = &line[line_pos];
    std::string type(name);
    if (type == "malloc") {
      // Format:
      //   TID: malloc POINTER SIZE_OF_ALLOCATION
      size_t size;
      if (sscanf(line_end, "%zu", &size) != 1) {
        errx(1, "Internal Error: Failed to read malloc data %s", line.c_str());
      }
      size_t idx = GetIndex(free_indices, max_ptrs);
      indices[pointer] = idx;
      entries->emplace_back(MALLOC, idx, size, 0);
    } else if (type == "free") {
      // Format:
      //   TID: free POINTER
      if (pointer != 0) {
        auto entry = indices.find(pointer);
        if (entry == indices.end()) {
          errx(1, "Internal Error: Unable to find free pointer %" PRIuPTR, pointer);
        }
        free_indices.push(entry->second);
        entries->emplace_back(FREE, entry->second + 1, 0, 0);
      } else {
        entries->emplace_back(FREE, 0, 0, 0);
      }
    } else if (type == "calloc") {
      // Format:
      //   TID: calloc POINTER ITEM_SIZE ITEM_COUNT
      size_t n_elements;
      size_t size;
      if (sscanf(line_end, "%zu %zu", &n_elements, &size) != 2) {
        errx(1, "Internal Error: Failed to read calloc data %s", line.c_str());
      }
      size_t idx = GetIndex(free_indices, max_ptrs);
      indices[pointer] = idx;
      entries->emplace_back(CALLOC, idx, size, n_elements);
    } else if (type == "realloc") {
      // Format:
      //   TID: calloc POINTER NEW_SIZE OLD_POINTER
      uintptr_t old_pointer;
      size_t size;
      if (sscanf(line_end, "%" SCNxPTR " %zu", &old_pointer, &size) != 2) {
        errx(1, "Internal Error: Failed to read realloc data %s", line.c_str());
      }
      size_t old_pointer_idx = 0;
      if (old_pointer != 0) {
        auto entry = indices.find(old_pointer);
        if (entry == indices.end()) {
          errx(1, "Internal Error: Failed to find realloc pointer %" PRIuPTR, old_pointer);
        }
        old_pointer_idx = entry->second;
        free_indices.push(old_pointer_idx);
      }
      size_t idx = GetIndex(free_indices, max_ptrs);
      indices[pointer] = idx;
      entries->emplace_back(REALLOC, idx, size, old_pointer_idx + 1);
    } else if (type == "memalign") {
      // Format:
      //   TID: memalign POINTER SIZE ALIGNMENT
      size_t align;
      size_t size;
      if (sscanf(line_end, "%zu %zu", &align, &size) != 2) {
        errx(1, "Internal Error: Failed to read memalign data %s", line.c_str());
      }
      size_t idx = GetIndex(free_indices, max_ptrs);
      indices[pointer] = idx;
      entries->emplace_back(MEMALIGN, idx, size, align);
    } else if (type != "thread_done") {
      errx(1, "Internal Error: Unknown type %s", line.c_str());
    }
  }

  cached_max_ptrs = *max_ptrs;
  return entries;
}

static __always_inline uint64_t Nanotime() {
  struct timespec t;
  t.tv_sec = t.tv_nsec = 0;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return static_cast<uint64_t>(t.tv_sec) * 1000000000LL + t.tv_nsec;
}

static __always_inline void MakeAllocationResident(void* ptr, size_t nbytes, int pagesize) {
  uint8_t* data = reinterpret_cast<uint8_t*>(ptr);
  for (size_t i = 0; i < nbytes; i += pagesize) {
    data[i] = 1;
  }
}

static void RunTrace(benchmark::State& state, std::vector<MallocEntry>& entries, size_t max_ptrs) {
  std::vector<void*> ptrs(max_ptrs, nullptr);

  int pagesize = getpagesize();
  void* ptr;
  uint64_t total_ns = 0;
  uint64_t start_ns;
  for (auto& entry : entries) {
    switch (entry.type) {
      case MALLOC:
        start_ns = Nanotime();
        ptr = malloc(entry.size);
        if (ptr == nullptr) {
          errx(1, "malloc returned nullptr");
        }
        MakeAllocationResident(ptr, entry.size, pagesize);
        total_ns += Nanotime() - start_ns;

        if (ptrs[entry.idx] != nullptr) {
          errx(1, "Internal Error: malloc pointer being replaced is not nullptr");
        }
        ptrs[entry.idx] = ptr;
        break;

      case CALLOC:
        start_ns = Nanotime();
        ptr = calloc(entry.arg2, entry.size);
        if (ptr == nullptr) {
          errx(1, "calloc returned nullptr");
        }
        MakeAllocationResident(ptr, entry.size, pagesize);
        total_ns += Nanotime() - start_ns;

        if (ptrs[entry.idx] != nullptr) {
          errx(1, "Internal Error: calloc pointer being replaced is not nullptr");
        }
        ptrs[entry.idx] = ptr;
        break;

      case MEMALIGN:
        start_ns = Nanotime();
        ptr = memalign(entry.arg2, entry.size);
        if (ptr == nullptr) {
          errx(1, "memalign returned nullptr");
        }
        MakeAllocationResident(ptr, entry.size, pagesize);
        total_ns += Nanotime() - start_ns;

        if (ptrs[entry.idx] != nullptr) {
          errx(1, "Internal Error: memalign pointer being replaced is not nullptr");
        }
        ptrs[entry.idx] = ptr;
        break;

      case REALLOC:
        start_ns = Nanotime();
        if (entry.arg2 == 0) {
          ptr = realloc(nullptr, entry.size);
        } else {
          ptr = realloc(ptrs[entry.arg2 - 1], entry.size);
          ptrs[entry.arg2 - 1] = nullptr;
        }
        if (entry.size > 0) {
          if (ptr == nullptr) {
            errx(1, "realloc returned nullptr");
          }
          MakeAllocationResident(ptr, entry.size, pagesize);
        }
        total_ns += Nanotime() - start_ns;

        if (ptrs[entry.idx] != nullptr) {
          errx(1, "Internal Error: realloc pointer being replaced is not nullptr");
        }
        ptrs[entry.idx] = ptr;
        break;

      case FREE:
        if (entry.idx != 0) {
          ptr = ptrs[entry.idx - 1];
          ptrs[entry.idx - 1] = nullptr;
        } else {
          ptr = nullptr;
        }
        start_ns = Nanotime();
        free(ptr);
        total_ns += Nanotime() - start_ns;
        break;
    }
  }
  state.SetIterationTime(total_ns / double(1000000000.0));

  std::for_each(ptrs.begin(), ptrs.end(), [](void* ptr) { free(ptr); });
}

// Run a trace as if all of the allocations occurred in a single thread.
// This is not completely realistic, but it is a possible worst case that
// could happen in an app.
static void BenchmarkTrace(benchmark::State& state, const char* filename) {
  std::string full_filename(android::base::GetExecutableDirectory() + "/traces/" + filename);
  size_t max_ptrs;
  std::vector<MallocEntry>* entries = GetTraceData(full_filename.c_str(), &max_ptrs);
  if (entries == nullptr) {
    errx(1, "ERROR: Failed to get trace data for %s.", full_filename.c_str());
  }

#if defined(__BIONIC__)
  // Need to set the decay time the same as how an app would operate.
  mallopt(M_DECAY_TIME, 1);
#endif

  for (auto _ : state) {
    RunTrace(state, *entries, max_ptrs);
  }
}

#define BENCH_OPTIONS                 \
  UseManualTime()                     \
      ->Unit(benchmark::kMicrosecond) \
      ->MinTime(15.0)                 \
      ->Repetitions(4)                \
      ->ReportAggregatesOnly(true)

static void BM_angry_birds2(benchmark::State& state) {
  BenchmarkTrace(state, "angry_birds2.zip");
}
BENCHMARK(BM_angry_birds2)->BENCH_OPTIONS;

static void BM_camera(benchmark::State& state) {
  BenchmarkTrace(state, "camera.zip");
}
BENCHMARK(BM_camera)->BENCH_OPTIONS;

static void BM_candy_crush_saga(benchmark::State& state) {
  BenchmarkTrace(state, "candy_crush_saga.zip");
}
BENCHMARK(BM_candy_crush_saga)->BENCH_OPTIONS;

void BM_gmail(benchmark::State& state) {
  BenchmarkTrace(state, "gmail.zip");
}
BENCHMARK(BM_gmail)->BENCH_OPTIONS;

void BM_maps(benchmark::State& state) {
  BenchmarkTrace(state, "maps.zip");
}
BENCHMARK(BM_maps)->BENCH_OPTIONS;

void BM_photos(benchmark::State& state) {
  BenchmarkTrace(state, "photos.zip");
}
BENCHMARK(BM_photos)->BENCH_OPTIONS;

void BM_pubg(benchmark::State& state) {
  BenchmarkTrace(state, "pubg.zip");
}
BENCHMARK(BM_pubg)->BENCH_OPTIONS;

void BM_surfaceflinger(benchmark::State& state) {
  BenchmarkTrace(state, "surfaceflinger.zip");
}
BENCHMARK(BM_surfaceflinger)->BENCH_OPTIONS;

void BM_system_server(benchmark::State& state) {
  BenchmarkTrace(state, "system_server.zip");
}
BENCHMARK(BM_system_server)->BENCH_OPTIONS;

void BM_systemui(benchmark::State& state) {
  BenchmarkTrace(state, "systemui.zip");
}
BENCHMARK(BM_systemui)->BENCH_OPTIONS;

void BM_youtube(benchmark::State& state) {
  BenchmarkTrace(state, "youtube.zip");
}
BENCHMARK(BM_youtube)->BENCH_OPTIONS;

int main(int argc, char** argv) {
  std::vector<char*> args;
  args.push_back(argv[0]);

  // Look for the --cpu=XX option.
  for (int i = 1; i < argc; i++) {
    if (strncmp(argv[i], "--cpu=", 6) == 0) {
      char* endptr;
      int cpu = strtol(&argv[i][6], &endptr, 10);
      if (argv[i][0] == '\0' || endptr == nullptr || *endptr != '\0') {
        printf("Invalid format of --cpu option, '%s' must be an integer value.\n", argv[i] + 6);
        return 1;
      }
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(cpu, &cpuset);
      if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
        if (errno == EINVAL) {
          printf("Invalid cpu %d\n", cpu);
          return 1;
        }
        perror("sched_setaffinity failed");
        return 1;
      }
      printf("Locking to cpu %d\n", cpu);
    } else {
      args.push_back(argv[i]);
    }
  }

  argc = args.size();
  ::benchmark::Initialize(&argc, args.data());
  if (::benchmark::ReportUnrecognizedArguments(argc, args.data())) return 1;
  ::benchmark::RunSpecifiedBenchmarks();
}
