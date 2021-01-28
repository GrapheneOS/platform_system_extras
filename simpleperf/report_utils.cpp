/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "report_utils.h"

#include <android-base/strings.h>

#include "JITDebugReader.h"

namespace simpleperf {

static bool IsArtDso(const Dso* dso) {
  return android::base::EndsWith(dso->Path(), "/libart.so") ||
         android::base::EndsWith(dso->Path(), "/libartd.so");
};

std::vector<CallChainReportEntry> CallChainReportBuilder::Build(const ThreadEntry* thread,
                                                                const std::vector<uint64_t>& ips,
                                                                size_t kernel_ip_count) {
  std::vector<CallChainReportEntry> result;
  result.reserve(ips.size());
  for (size_t i = 0; i < ips.size(); i++) {
    const MapEntry* map = thread_tree_.FindMap(thread, ips[i], i < kernel_ip_count);
    Dso* dso = map->dso;
    CallChainExecutionType execution_type = CallChainExecutionType::NATIVE_METHOD;
    if (dso->IsForJavaMethod()) {
      if (dso->type() == DSO_DEX_FILE) {
        execution_type = CallChainExecutionType::INTERPRETED_JVM_METHOD;
      } else {
        execution_type = CallChainExecutionType::JIT_JVM_METHOD;
      }
    }
    uint64_t vaddr_in_file;
    const Symbol* symbol = thread_tree_.FindSymbol(map, ips[i], &vaddr_in_file, &dso);
    result.resize(result.size() + 1);
    auto& entry = result.back();
    entry.ip = ips[i];
    entry.symbol = symbol;
    entry.dso = dso;
    entry.vaddr_in_file = vaddr_in_file;
    entry.map = map;
    entry.execution_type = execution_type;
  }
  MarkArtFrame(result);
  if (remove_art_frame_) {
    auto it = std::remove_if(result.begin(), result.end(), [](const CallChainReportEntry& entry) {
      return entry.execution_type == CallChainExecutionType::ART_METHOD;
    });
    result.erase(it, result.end());
  }
  if (convert_jit_frame_) {
    ConvertJITFrame(result);
  }
  return result;
}

void CallChainReportBuilder::MarkArtFrame(std::vector<CallChainReportEntry>& callchain) {
  // Mark art methods before or after a JVM method.
  bool near_java_method = false;
  for (size_t i = 0; i < callchain.size(); ++i) {
    auto& entry = callchain[i];
    if (entry.execution_type == CallChainExecutionType::INTERPRETED_JVM_METHOD ||
        entry.execution_type == CallChainExecutionType::JIT_JVM_METHOD) {
      near_java_method = true;

      // Mark art frames before this entry.
      for (int j = static_cast<int>(i) - 1; j >= 0; j--) {
        if (!IsArtDso(callchain[j].dso)) {
          break;
        }
        callchain[j].execution_type = CallChainExecutionType::ART_METHOD;
      }
    } else if (near_java_method && IsArtDso(entry.dso)) {
      entry.execution_type = CallChainExecutionType::ART_METHOD;
    } else {
      near_java_method = false;
    }
  }
}

void CallChainReportBuilder::ConvertJITFrame(std::vector<CallChainReportEntry>& callchain) {
  CollectJavaMethods();
  for (size_t i = 0; i < callchain.size();) {
    auto& entry = callchain[i];
    if (entry.dso->IsForJavaMethod() && entry.dso->type() == DSO_ELF_FILE) {
      // This is a JIT java method, merge it with the interpreted java method having the same
      // name if possible. Otherwise, merge it with other JIT java methods having the same name
      // by assigning a common dso_name.
      if (auto it = java_method_map_.find(entry.symbol->Name()); it != java_method_map_.end()) {
        entry.dso = it->second.dso;
        entry.symbol = it->second.symbol;
        // Not enough info to map an offset in a JIT method to an offset in a dex file. So just
        // use the symbol_addr.
        entry.vaddr_in_file = entry.symbol->addr;

        // ART may call from an interpreted Java method into its corresponding JIT method. To avoid
        // showing the method calling itself, remove the JIT frame.
        if (i + 1 < callchain.size() && callchain[i + 1].dso == entry.dso &&
            callchain[i + 1].symbol == entry.symbol) {
          callchain.erase(callchain.begin() + i);
          continue;
        }

      } else if (!JITDebugReader::IsPathInJITSymFile(entry.dso->Path())) {
        // Old JITSymFiles use names like "TemporaryFile-XXXXXX". So give them a better name.
        entry.dso_name = "[JIT cache]";
      }
    }
    i++;
  }
}

void CallChainReportBuilder::CollectJavaMethods() {
  if (!java_method_initialized_) {
    java_method_initialized_ = true;
    for (Dso* dso : thread_tree_.GetAllDsos()) {
      if (dso->type() == DSO_DEX_FILE) {
        dso->LoadSymbols();
        for (auto& symbol : dso->GetSymbols()) {
          java_method_map_.emplace(symbol.Name(), JavaMethod(dso, &symbol));
        }
      }
    }
  }
}

}  // namespace simpleperf
