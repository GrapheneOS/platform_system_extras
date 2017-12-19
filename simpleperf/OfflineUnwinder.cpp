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

#include "OfflineUnwinder.h"

#include <ucontext.h>

#include <android-base/logging.h>
#include <backtrace/Backtrace.h>

#include "environment.h"
#include "thread_tree.h"

namespace simpleperf {

#define SetUContextReg(dst, perf_regno)          \
  do {                                           \
    uint64_t value;                              \
    if (GetRegValue(regs, perf_regno, &value)) { \
      (dst) = value;                             \
    }                                            \
  } while (0)

static ucontext_t BuildUContextFromRegs(const RegSet& regs __attribute__((unused))) {
  ucontext_t ucontext;
  memset(&ucontext, 0, sizeof(ucontext));
#if defined(__i386__)
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_GS], PERF_REG_X86_GS);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_FS], PERF_REG_X86_FS);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_ES], PERF_REG_X86_ES);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_DS], PERF_REG_X86_DS);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_EAX], PERF_REG_X86_AX);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_EBX], PERF_REG_X86_BX);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_ECX], PERF_REG_X86_CX);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_EDX], PERF_REG_X86_DX);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_ESI], PERF_REG_X86_SI);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_EDI], PERF_REG_X86_DI);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_EBP], PERF_REG_X86_BP);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_EIP], PERF_REG_X86_IP);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_ESP], PERF_REG_X86_SP);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_CS], PERF_REG_X86_CS);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_EFL], PERF_REG_X86_FLAGS);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_SS], PERF_REG_X86_SS);
#elif defined(__x86_64__)
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_R8], PERF_REG_X86_R8);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_R9], PERF_REG_X86_R9);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_R10], PERF_REG_X86_R10);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_R11], PERF_REG_X86_R11);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_R12], PERF_REG_X86_R12);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_R13], PERF_REG_X86_R13);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_R14], PERF_REG_X86_R14);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_R15], PERF_REG_X86_R15);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_RDI], PERF_REG_X86_DI);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_RSI], PERF_REG_X86_SI);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_RBP], PERF_REG_X86_BP);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_RBX], PERF_REG_X86_BX);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_RDX], PERF_REG_X86_DX);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_RAX], PERF_REG_X86_AX);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_RCX], PERF_REG_X86_CX);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_RSP], PERF_REG_X86_SP);
  SetUContextReg(ucontext.uc_mcontext.gregs[REG_RIP], PERF_REG_X86_IP);
#elif defined(__aarch64__)
  for (size_t i = PERF_REG_ARM64_X0; i < PERF_REG_ARM64_MAX; ++i) {
    SetUContextReg(ucontext.uc_mcontext.regs[i], i);
  }
#elif defined(__arm__)
  SetUContextReg(ucontext.uc_mcontext.arm_r0, PERF_REG_ARM_R0);
  SetUContextReg(ucontext.uc_mcontext.arm_r1, PERF_REG_ARM_R1);
  SetUContextReg(ucontext.uc_mcontext.arm_r2, PERF_REG_ARM_R2);
  SetUContextReg(ucontext.uc_mcontext.arm_r3, PERF_REG_ARM_R3);
  SetUContextReg(ucontext.uc_mcontext.arm_r4, PERF_REG_ARM_R4);
  SetUContextReg(ucontext.uc_mcontext.arm_r5, PERF_REG_ARM_R5);
  SetUContextReg(ucontext.uc_mcontext.arm_r6, PERF_REG_ARM_R6);
  SetUContextReg(ucontext.uc_mcontext.arm_r7, PERF_REG_ARM_R7);
  SetUContextReg(ucontext.uc_mcontext.arm_r8, PERF_REG_ARM_R8);
  SetUContextReg(ucontext.uc_mcontext.arm_r9, PERF_REG_ARM_R9);
  SetUContextReg(ucontext.uc_mcontext.arm_r10, PERF_REG_ARM_R10);
  SetUContextReg(ucontext.uc_mcontext.arm_fp, PERF_REG_ARM_FP);
  SetUContextReg(ucontext.uc_mcontext.arm_ip, PERF_REG_ARM_IP);
  SetUContextReg(ucontext.uc_mcontext.arm_sp, PERF_REG_ARM_SP);
  SetUContextReg(ucontext.uc_mcontext.arm_lr, PERF_REG_ARM_LR);
  SetUContextReg(ucontext.uc_mcontext.arm_pc, PERF_REG_ARM_PC);
#endif
  return ucontext;
}

bool OfflineUnwinder::UnwindCallChain(int abi, const ThreadEntry& thread, const RegSet& regs,
                                      const char* stack, size_t stack_size,
                                      std::vector<uint64_t>* ips, std::vector<uint64_t>* sps) {
  uint64_t start_time;
  if (collect_stat_) {
    start_time = GetSystemClock();
  }
  std::vector<uint64_t> result;
  ArchType arch = (abi != PERF_SAMPLE_REGS_ABI_32) ?
                      ScopedCurrentArch::GetCurrentArch() :
                      ScopedCurrentArch::GetCurrentArch32();
  if (!IsArchTheSame(arch, GetBuildArch(), strict_arch_check_)) {
    LOG(ERROR) << "simpleperf is built in arch " << GetArchString(GetBuildArch())
                << ", and can't do stack unwinding for arch " << GetArchString(arch);
    return false;
  }
  uint64_t sp_reg_value;
  if (!GetSpRegValue(regs, arch, &sp_reg_value)) {
    LOG(ERROR) << "can't get sp reg value";
    return false;
  }
  if (arch != GetBuildArch()) {
    uint64_t ip_reg_value;
    if (!GetIpRegValue(regs, arch, &ip_reg_value)) {
      LOG(ERROR) << "can't get ip reg value";
      return false;
    }
    ips->push_back(ip_reg_value);
    sps->push_back(sp_reg_value);
    if (collect_stat_) {
      unwinding_result_.used_time = GetSystemClock() - start_time;
      unwinding_result_.stop_reason = UnwindingResult::DIFFERENT_ARCH;
    }
    return true;
  }
  uint64_t stack_addr = sp_reg_value;

  std::vector<backtrace_map_t> bt_maps(thread.maps->size());
  size_t map_index = 0;
  for (auto& map : *thread.maps) {
    backtrace_map_t& bt_map = bt_maps[map_index++];
    bt_map.start = map->start_addr;
    bt_map.end = map->start_addr + map->len;
    bt_map.offset = map->pgoff;
    bt_map.name = map->dso->GetDebugFilePath();
    bt_map.flags = PROT_READ | PROT_EXEC;
  }
  std::unique_ptr<BacktraceMap> backtrace_map(BacktraceMap::Create(thread.pid, bt_maps));

  backtrace_stackinfo_t stack_info;
  stack_info.start = stack_addr;
  stack_info.end = stack_addr + stack_size;
  stack_info.data = reinterpret_cast<const uint8_t*>(stack);

  std::unique_ptr<Backtrace> backtrace(
      Backtrace::CreateOffline(thread.pid, thread.tid, backtrace_map.get(), stack_info, true));
  ucontext_t ucontext = BuildUContextFromRegs(regs);
  if (backtrace->Unwind(0, &ucontext)) {
    for (auto it = backtrace->begin(); it != backtrace->end(); ++it) {
      // Unwinding in arm architecture can return 0 pc address.
      if (it->pc == 0) {
        break;
      }
      ips->push_back(it->pc);
      sps->push_back(it->sp);
    }
  }
  if (ips->empty()) {
    return false;
  }
  if (collect_stat_) {
    unwinding_result_.used_time = GetSystemClock() - start_time;
    BacktraceUnwindError error = backtrace->GetError();
    switch (error.error_code) {
      case BACKTRACE_UNWIND_ERROR_EXCEED_MAX_FRAMES_LIMIT:
        unwinding_result_.stop_reason = UnwindingResult::EXCEED_MAX_FRAMES_LIMIT;
        break;
      case BACKTRACE_UNWIND_ERROR_ACCESS_REG_FAILED:
        unwinding_result_.stop_reason = UnwindingResult::ACCESS_REG_FAILED;
        unwinding_result_.stop_info.regno = error.error_info.regno;
        break;
      case BACKTRACE_UNWIND_ERROR_ACCESS_MEM_FAILED:
        // Because we don't have precise stack range here, just guess an addr is in stack
        // if sp - 128K <= addr <= sp.
        if (error.error_info.addr <= stack_addr &&
            error.error_info.addr >= stack_addr - 128 * 1024) {
          unwinding_result_.stop_reason = UnwindingResult::ACCESS_STACK_FAILED;
        } else {
          unwinding_result_.stop_reason = UnwindingResult::ACCESS_MEM_FAILED;
        }
        unwinding_result_.stop_info.addr = error.error_info.addr;
        break;
      case BACKTRACE_UNWIND_ERROR_FIND_PROC_INFO_FAILED:
        unwinding_result_.stop_reason = UnwindingResult::FIND_PROC_INFO_FAILED;
        break;
      case BACKTRACE_UNWIND_ERROR_EXECUTE_DWARF_INSTRUCTION_FAILED:
        unwinding_result_.stop_reason = UnwindingResult::EXECUTE_DWARF_INSTRUCTION_FAILED;
        break;
      case BACKTRACE_UNWIND_ERROR_MAP_MISSING:
        unwinding_result_.stop_reason = UnwindingResult::MAP_MISSING;
        break;
      default:
        unwinding_result_.stop_reason = UnwindingResult::UNKNOWN_REASON;
        break;
    }
  }
  return true;
}

}  // namespace simpleperf
