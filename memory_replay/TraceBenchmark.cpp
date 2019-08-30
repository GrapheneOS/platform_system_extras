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
#include <unistd.h>

#include <algorithm>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include <android-base/file.h>
#include <android-base/strings.h>
#include <benchmark/benchmark.h>

#include "Alloc.h"
#include "Utils.h"
#include "Zip.h"

struct TraceAllocEntry {
  TraceAllocEntry(AllocEnum type, size_t idx, size_t size, size_t last_arg)
      : type(type), idx(idx), size(size) {
    u.old_idx = last_arg;
  }
  AllocEnum type;
  size_t idx;
  size_t size;
  union {
    size_t old_idx = 0;
    size_t align;
    size_t n_elements;
  } u;
};

static size_t GetIndex(std::stack<size_t>& free_indices, size_t* max_index) {
  if (free_indices.empty()) {
    return (*max_index)++;
  }
  size_t index = free_indices.top();
  free_indices.pop();
  return index;
}

static std::vector<TraceAllocEntry>* GetTraceData(const char* filename, size_t* max_ptrs) {
  // Only keep last trace encountered cached.
  static std::string cached_filename;
  static std::vector<TraceAllocEntry> cached_entries;
  static size_t cached_max_ptrs;

  if (cached_filename == filename) {
    *max_ptrs = cached_max_ptrs;
    return &cached_entries;
  }

  cached_entries.clear();
  cached_max_ptrs = 0;
  cached_filename = filename;

  std::string content(ZipGetContents(filename));
  if (content.empty()) {
    errx(1, "Internal Error: Empty zip file %s", filename);
  }
  std::vector<std::string> lines(android::base::Split(content, "\n"));

  *max_ptrs = 0;
  std::stack<size_t> free_indices;
  std::unordered_map<uint64_t, size_t> ptr_to_index;
  std::vector<TraceAllocEntry>* entries = &cached_entries;
  for (const std::string& line : lines) {
    if (line.empty()) {
      continue;
    }
    AllocEntry entry;
    AllocGetData(line, &entry);

    switch (entry.type) {
      case MALLOC: {
        size_t idx = GetIndex(free_indices, max_ptrs);
        ptr_to_index[entry.ptr] = idx;
        entries->emplace_back(MALLOC, idx, entry.size, 0);
        break;
      }
      case CALLOC: {
        size_t idx = GetIndex(free_indices, max_ptrs);
        ptr_to_index[entry.ptr] = idx;
        entries->emplace_back(CALLOC, idx, entry.u.n_elements, entry.size);
        break;
      }
      case MEMALIGN: {
        size_t idx = GetIndex(free_indices, max_ptrs);
        ptr_to_index[entry.ptr] = idx;
        entries->emplace_back(MEMALIGN, idx, entry.size, entry.u.align);
        break;
      }
      case REALLOC: {
        size_t old_pointer_idx = 0;
        if (entry.u.old_ptr != 0) {
          auto idx_entry = ptr_to_index.find(entry.u.old_ptr);
          if (idx_entry == ptr_to_index.end()) {
            errx(1, "File Error: Failed to find realloc pointer %" PRIu64, entry.u.old_ptr);
          }
          old_pointer_idx = idx_entry->second;
          free_indices.push(old_pointer_idx);
        }
        size_t idx = GetIndex(free_indices, max_ptrs);
        ptr_to_index[entry.ptr] = idx;
        entries->emplace_back(REALLOC, idx, entry.size, old_pointer_idx + 1);
        break;
      }
      case FREE:
        if (entry.ptr != 0) {
          auto idx_entry = ptr_to_index.find(entry.ptr);
          if (idx_entry == ptr_to_index.end()) {
            errx(1, "File Error: Unable to find free pointer %" PRIu64, entry.ptr);
          }
          free_indices.push(idx_entry->second);
          entries->emplace_back(FREE, idx_entry->second + 1, 0, 0);
        } else {
          entries->emplace_back(FREE, 0, 0, 0);
        }
        break;
      case THREAD_DONE:
        // Ignore these.
        break;
    }
  }

  cached_max_ptrs = *max_ptrs;
  return entries;
}

static void RunTrace(benchmark::State& state, std::vector<TraceAllocEntry>& entries,
                     size_t max_ptrs) {
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
        ptr = calloc(entry.u.n_elements, entry.size);
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
        ptr = memalign(entry.u.align, entry.size);
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
        if (entry.u.old_idx == 0) {
          ptr = realloc(nullptr, entry.size);
        } else {
          ptr = realloc(ptrs[entry.u.old_idx - 1], entry.size);
          ptrs[entry.u.old_idx - 1] = nullptr;
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

      case THREAD_DONE:
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
  std::vector<TraceAllocEntry>* entries = GetTraceData(full_filename.c_str(), &max_ptrs);
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
