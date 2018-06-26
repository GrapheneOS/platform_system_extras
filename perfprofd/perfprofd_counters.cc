/*
 *
 * Copyright 2015, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfprofd_counters.h"

#include "perfprofd_config.pb.h"

namespace android {
namespace perfprofd {

std::vector<const char*> GenerateEventsString(const CounterSet& counter_set) {
  std::vector<const char*> result;

  if (counter_set.has_branch_load_misses() && counter_set.branch_load_misses())
    result.push_back("branch-load-misses");
  if (counter_set.has_branch_loads() && counter_set.branch_loads())
    result.push_back("branch-loads");
  if (counter_set.has_branch_store_misses() && counter_set.branch_store_misses())
    result.push_back("branch-store-misses");
  if (counter_set.has_branch_stores() && counter_set.branch_stores())
    result.push_back("branch-stores");
  if (counter_set.has_l1_dcache_load_misses() && counter_set.l1_dcache_load_misses())
    result.push_back("L1-dcache-load-misses");
  if (counter_set.has_l1_dcache_loads() && counter_set.l1_dcache_loads())
    result.push_back("L1-dcache-loads");
  if (counter_set.has_l1_dcache_store_misses() && counter_set.l1_dcache_store_misses())
    result.push_back("L1-dcache-store-misses");
  if (counter_set.has_l1_dcache_stores() && counter_set.l1_dcache_stores())
    result.push_back("L1-dcache-stores");
  if (counter_set.has_branch_misses() && counter_set.branch_misses())
    result.push_back("branch-misses");
  if (counter_set.has_cache_misses() && counter_set.cache_misses())
    result.push_back("cache-misses");
  if (counter_set.has_cache_references() && counter_set.cache_references())
    result.push_back("cache-references");
  if (counter_set.has_cpu_cycles() && counter_set.cpu_cycles())
    result.push_back("cpu-cycles");
  if (counter_set.has_instructions() && counter_set.instructions())
    result.push_back("instructions");
  if (counter_set.has_raw_br_mis_pred() && counter_set.raw_br_mis_pred())
    result.push_back("raw-br-mis-pred");
  if (counter_set.has_raw_br_mis_pred_retired() && counter_set.raw_br_mis_pred_retired())
    result.push_back("raw-br-mis-pred-retired");
  if (counter_set.has_raw_br_pred() && counter_set.raw_br_pred())
    result.push_back("raw-br-pred");
  if (counter_set.has_raw_br_retired() && counter_set.raw_br_retired())
    result.push_back("raw-br-retired");
  if (counter_set.has_raw_bus_access() && counter_set.raw_bus_access())
    result.push_back("raw-bus-access");
  if (counter_set.has_raw_cpu_cycles() && counter_set.raw_cpu_cycles())
    result.push_back("raw-cpu-cycles");
  if (counter_set.has_raw_exception_return() && counter_set.raw_exception_return())
    result.push_back("raw-exception-return");
  if (counter_set.has_raw_exception_taken() && counter_set.raw_exception_taken())
    result.push_back("raw-exception-taken");
  if (counter_set.has_raw_inst_spec() && counter_set.raw_inst_spec())
    result.push_back("raw-inst-spec");
  if (counter_set.has_raw_instruction_retired() && counter_set.raw_instruction_retired())
    result.push_back("raw-instruction-retired");
  if (counter_set.has_raw_l1_dcache() && counter_set.raw_l1_dcache())
    result.push_back("raw-l1-dcache");
  if (counter_set.has_raw_l1_dcache_allocate() && counter_set.raw_l1_dcache_allocate())
    result.push_back("raw-l1-dcache-allocate");
  if (counter_set.has_raw_l1_dcache_refill() && counter_set.raw_l1_dcache_refill())
    result.push_back("raw-l1-dcache-refill");
  if (counter_set.has_raw_l1_dtlb() && counter_set.raw_l1_dtlb())
    result.push_back("raw-l1-dtlb");
  if (counter_set.has_raw_l1_dtlb_refill() && counter_set.raw_l1_dtlb_refill())
    result.push_back("raw-l1-dtlb-refill");
  if (counter_set.has_raw_l1_icache() && counter_set.raw_l1_icache())
    result.push_back("raw-l1-icache");
  if (counter_set.has_raw_l1_icache_refill() && counter_set.raw_l1_icache_refill())
    result.push_back("raw-l1-icache-refill");
  if (counter_set.has_raw_l1_itlb_refill() && counter_set.raw_l1_itlb_refill())
    result.push_back("raw-l1-itlb-refill");
  if (counter_set.has_raw_l2_dcache() && counter_set.raw_l2_dcache())
    result.push_back("raw-l2-dcache");
  if (counter_set.has_raw_l2_dcache_allocate() && counter_set.raw_l2_dcache_allocate())
    result.push_back("raw-l2-dcache-allocate");
  if (counter_set.has_raw_l2_dcache_refill() && counter_set.raw_l2_dcache_refill())
    result.push_back("raw-l2-dcache-refill");
  if (counter_set.has_raw_l2_dcache_wb() && counter_set.raw_l2_dcache_wb())
    result.push_back("raw-l2-dcache-wb");
  if (counter_set.has_raw_l2_icache_refill() && counter_set.raw_l2_icache_refill())
    result.push_back("raw-l2-icache-refill");
  if (counter_set.has_raw_l2_itlb() && counter_set.raw_l2_itlb())
    result.push_back("raw-l2-itlb");
  if (counter_set.has_raw_l3_dcache_allocate() && counter_set.raw_l3_dcache_allocate())
    result.push_back("raw-l3-dcache-allocate");
  if (counter_set.has_raw_l3_dcache_refill() && counter_set.raw_l3_dcache_refill())
    result.push_back("raw-l3-dcache-refill");
  if (counter_set.has_raw_mem_access() && counter_set.raw_mem_access())
    result.push_back("raw-mem-access");
  if (counter_set.has_raw_stall_backend() && counter_set.raw_stall_backend())
    result.push_back("raw-stall-backend");
  if (counter_set.has_raw_stall_frontend() && counter_set.raw_stall_frontend())
    result.push_back("raw-stall-frontend");
  if (counter_set.has_raw_ttbr_write_retired() && counter_set.raw_ttbr_write_retired())
    result.push_back("raw-ttbr-write-retired");
  if (counter_set.has_alignment_faults() && counter_set.alignment_faults())
    result.push_back("alignment-faults");
  if (counter_set.has_context_switches() && counter_set.context_switches())
    result.push_back("context-switches");
  if (counter_set.has_cpu_clock() && counter_set.cpu_clock())
    result.push_back("cpu-clock");
  if (counter_set.has_cpu_migrations() && counter_set.cpu_migrations())
    result.push_back("cpu-migrations");
  if (counter_set.has_emulation_faults() && counter_set.emulation_faults())
    result.push_back("emulation-faults");
  if (counter_set.has_major_faults() && counter_set.major_faults())
    result.push_back("major-faults");
  if (counter_set.has_minor_faults() && counter_set.minor_faults())
    result.push_back("minor-faults");
  if (counter_set.has_page_faults() && counter_set.page_faults())
    result.push_back("page-faults");
  if (counter_set.has_task_clock() && counter_set.task_clock())
    result.push_back("task-clock");

  if (counter_set.has_tracepoints()) {
    const auto& tracepoints = counter_set.tracepoints();


    if (tracepoints.has_asoc()) {
      const auto& asoc = tracepoints.asoc();

      if (asoc.has_snd_soc_bias_level_done() && asoc.snd_soc_bias_level_done())
        result.push_back("asoc:snd_soc_bias_level_done");
      if (asoc.has_snd_soc_bias_level_start() && asoc.snd_soc_bias_level_start())
        result.push_back("asoc:snd_soc_bias_level_start");
      if (asoc.has_snd_soc_cache_sync() && asoc.snd_soc_cache_sync())
        result.push_back("asoc:snd_soc_cache_sync");
      if (asoc.has_snd_soc_dapm_connected() && asoc.snd_soc_dapm_connected())
        result.push_back("asoc:snd_soc_dapm_connected");
      if (asoc.has_snd_soc_dapm_done() && asoc.snd_soc_dapm_done())
        result.push_back("asoc:snd_soc_dapm_done");
      if (asoc.has_snd_soc_dapm_input_path() && asoc.snd_soc_dapm_input_path())
        result.push_back("asoc:snd_soc_dapm_input_path");
      if (asoc.has_snd_soc_dapm_output_path() && asoc.snd_soc_dapm_output_path())
        result.push_back("asoc:snd_soc_dapm_output_path");
      if (asoc.has_snd_soc_dapm_start() && asoc.snd_soc_dapm_start())
        result.push_back("asoc:snd_soc_dapm_start");
      if (asoc.has_snd_soc_dapm_walk_done() && asoc.snd_soc_dapm_walk_done())
        result.push_back("asoc:snd_soc_dapm_walk_done");
      if (asoc.has_snd_soc_dapm_widget_event_done() && asoc.snd_soc_dapm_widget_event_done())
        result.push_back("asoc:snd_soc_dapm_widget_event_done");
      if (asoc.has_snd_soc_dapm_widget_event_start() && asoc.snd_soc_dapm_widget_event_start())
        result.push_back("asoc:snd_soc_dapm_widget_event_start");
      if (asoc.has_snd_soc_dapm_widget_power() && asoc.snd_soc_dapm_widget_power())
        result.push_back("asoc:snd_soc_dapm_widget_power");
      if (asoc.has_snd_soc_jack_irq() && asoc.snd_soc_jack_irq())
        result.push_back("asoc:snd_soc_jack_irq");
      if (asoc.has_snd_soc_jack_notify() && asoc.snd_soc_jack_notify())
        result.push_back("asoc:snd_soc_jack_notify");
      if (asoc.has_snd_soc_jack_report() && asoc.snd_soc_jack_report())
        result.push_back("asoc:snd_soc_jack_report");
    }

    if (tracepoints.has_binder()) {
      const auto& binder = tracepoints.binder();

      if (binder.has_binder_alloc_lru_end() && binder.binder_alloc_lru_end())
        result.push_back("binder:binder_alloc_lru_end");
      if (binder.has_binder_alloc_lru_start() && binder.binder_alloc_lru_start())
        result.push_back("binder:binder_alloc_lru_start");
      if (binder.has_binder_alloc_page_end() && binder.binder_alloc_page_end())
        result.push_back("binder:binder_alloc_page_end");
      if (binder.has_binder_alloc_page_start() && binder.binder_alloc_page_start())
        result.push_back("binder:binder_alloc_page_start");
      if (binder.has_binder_command() && binder.binder_command())
        result.push_back("binder:binder_command");
      if (binder.has_binder_free_lru_end() && binder.binder_free_lru_end())
        result.push_back("binder:binder_free_lru_end");
      if (binder.has_binder_free_lru_start() && binder.binder_free_lru_start())
        result.push_back("binder:binder_free_lru_start");
      if (binder.has_binder_ioctl() && binder.binder_ioctl())
        result.push_back("binder:binder_ioctl");
      if (binder.has_binder_ioctl_done() && binder.binder_ioctl_done())
        result.push_back("binder:binder_ioctl_done");
      if (binder.has_binder_lock() && binder.binder_lock())
        result.push_back("binder:binder_lock");
      if (binder.has_binder_locked() && binder.binder_locked())
        result.push_back("binder:binder_locked");
      if (binder.has_binder_read_done() && binder.binder_read_done())
        result.push_back("binder:binder_read_done");
      if (binder.has_binder_return() && binder.binder_return())
        result.push_back("binder:binder_return");
      if (binder.has_binder_set_priority() && binder.binder_set_priority())
        result.push_back("binder:binder_set_priority");
      if (binder.has_binder_transaction() && binder.binder_transaction())
        result.push_back("binder:binder_transaction");
      if (binder.has_binder_transaction_alloc_buf() && binder.binder_transaction_alloc_buf())
        result.push_back("binder:binder_transaction_alloc_buf");
      if (binder.has_binder_transaction_buffer_release() && binder.binder_transaction_buffer_release())
        result.push_back("binder:binder_transaction_buffer_release");
      if (binder.has_binder_transaction_failed_buffer_release() && binder.binder_transaction_failed_buffer_release())
        result.push_back("binder:binder_transaction_failed_buffer_release");
      if (binder.has_binder_transaction_fd() && binder.binder_transaction_fd())
        result.push_back("binder:binder_transaction_fd");
      if (binder.has_binder_transaction_node_to_ref() && binder.binder_transaction_node_to_ref())
        result.push_back("binder:binder_transaction_node_to_ref");
      if (binder.has_binder_transaction_received() && binder.binder_transaction_received())
        result.push_back("binder:binder_transaction_received");
      if (binder.has_binder_transaction_ref_to_node() && binder.binder_transaction_ref_to_node())
        result.push_back("binder:binder_transaction_ref_to_node");
      if (binder.has_binder_transaction_ref_to_ref() && binder.binder_transaction_ref_to_ref())
        result.push_back("binder:binder_transaction_ref_to_ref");
      if (binder.has_binder_unlock() && binder.binder_unlock())
        result.push_back("binder:binder_unlock");
      if (binder.has_binder_unmap_kernel_end() && binder.binder_unmap_kernel_end())
        result.push_back("binder:binder_unmap_kernel_end");
      if (binder.has_binder_unmap_kernel_start() && binder.binder_unmap_kernel_start())
        result.push_back("binder:binder_unmap_kernel_start");
      if (binder.has_binder_unmap_user_end() && binder.binder_unmap_user_end())
        result.push_back("binder:binder_unmap_user_end");
      if (binder.has_binder_unmap_user_start() && binder.binder_unmap_user_start())
        result.push_back("binder:binder_unmap_user_start");
      if (binder.has_binder_update_page_range() && binder.binder_update_page_range())
        result.push_back("binder:binder_update_page_range");
      if (binder.has_binder_wait_for_work() && binder.binder_wait_for_work())
        result.push_back("binder:binder_wait_for_work");
      if (binder.has_binder_write_done() && binder.binder_write_done())
        result.push_back("binder:binder_write_done");
    }

    if (tracepoints.has_block()) {
      const auto& block = tracepoints.block();

      if (block.has_block_bio_backmerge() && block.block_bio_backmerge())
        result.push_back("block:block_bio_backmerge");
      if (block.has_block_bio_bounce() && block.block_bio_bounce())
        result.push_back("block:block_bio_bounce");
      if (block.has_block_bio_complete() && block.block_bio_complete())
        result.push_back("block:block_bio_complete");
      if (block.has_block_bio_frontmerge() && block.block_bio_frontmerge())
        result.push_back("block:block_bio_frontmerge");
      if (block.has_block_bio_queue() && block.block_bio_queue())
        result.push_back("block:block_bio_queue");
      if (block.has_block_bio_remap() && block.block_bio_remap())
        result.push_back("block:block_bio_remap");
      if (block.has_block_dirty_buffer() && block.block_dirty_buffer())
        result.push_back("block:block_dirty_buffer");
      if (block.has_block_getrq() && block.block_getrq())
        result.push_back("block:block_getrq");
      if (block.has_block_plug() && block.block_plug())
        result.push_back("block:block_plug");
      if (block.has_block_rq_abort() && block.block_rq_abort())
        result.push_back("block:block_rq_abort");
      if (block.has_block_rq_complete() && block.block_rq_complete())
        result.push_back("block:block_rq_complete");
      if (block.has_block_rq_insert() && block.block_rq_insert())
        result.push_back("block:block_rq_insert");
      if (block.has_block_rq_issue() && block.block_rq_issue())
        result.push_back("block:block_rq_issue");
      if (block.has_block_rq_remap() && block.block_rq_remap())
        result.push_back("block:block_rq_remap");
      if (block.has_block_rq_requeue() && block.block_rq_requeue())
        result.push_back("block:block_rq_requeue");
      if (block.has_block_sleeprq() && block.block_sleeprq())
        result.push_back("block:block_sleeprq");
      if (block.has_block_split() && block.block_split())
        result.push_back("block:block_split");
      if (block.has_block_touch_buffer() && block.block_touch_buffer())
        result.push_back("block:block_touch_buffer");
      if (block.has_block_unplug() && block.block_unplug())
        result.push_back("block:block_unplug");
    }

    if (tracepoints.has_cfg80211()) {
      const auto& cfg80211 = tracepoints.cfg80211();

      if (cfg80211.has_cfg80211_cac_event() && cfg80211.cfg80211_cac_event())
        result.push_back("cfg80211:cfg80211_cac_event");
      if (cfg80211.has_cfg80211_ch_switch_notify() && cfg80211.cfg80211_ch_switch_notify())
        result.push_back("cfg80211:cfg80211_ch_switch_notify");
      if (cfg80211.has_cfg80211_chandef_dfs_required() && cfg80211.cfg80211_chandef_dfs_required())
        result.push_back("cfg80211:cfg80211_chandef_dfs_required");
      if (cfg80211.has_cfg80211_cqm_pktloss_notify() && cfg80211.cfg80211_cqm_pktloss_notify())
        result.push_back("cfg80211:cfg80211_cqm_pktloss_notify");
      if (cfg80211.has_cfg80211_cqm_rssi_notify() && cfg80211.cfg80211_cqm_rssi_notify())
        result.push_back("cfg80211:cfg80211_cqm_rssi_notify");
      if (cfg80211.has_cfg80211_del_sta() && cfg80211.cfg80211_del_sta())
        result.push_back("cfg80211:cfg80211_del_sta");
      if (cfg80211.has_cfg80211_ft_event() && cfg80211.cfg80211_ft_event())
        result.push_back("cfg80211:cfg80211_ft_event");
      if (cfg80211.has_cfg80211_get_bss() && cfg80211.cfg80211_get_bss())
        result.push_back("cfg80211:cfg80211_get_bss");
      if (cfg80211.has_cfg80211_gtk_rekey_notify() && cfg80211.cfg80211_gtk_rekey_notify())
        result.push_back("cfg80211:cfg80211_gtk_rekey_notify");
      if (cfg80211.has_cfg80211_ibss_joined() && cfg80211.cfg80211_ibss_joined())
        result.push_back("cfg80211:cfg80211_ibss_joined");
      if (cfg80211.has_cfg80211_inform_bss_frame() && cfg80211.cfg80211_inform_bss_frame())
        result.push_back("cfg80211:cfg80211_inform_bss_frame");
      if (cfg80211.has_cfg80211_mgmt_tx_status() && cfg80211.cfg80211_mgmt_tx_status())
        result.push_back("cfg80211:cfg80211_mgmt_tx_status");
      if (cfg80211.has_cfg80211_michael_mic_failure() && cfg80211.cfg80211_michael_mic_failure())
        result.push_back("cfg80211:cfg80211_michael_mic_failure");
      if (cfg80211.has_cfg80211_new_sta() && cfg80211.cfg80211_new_sta())
        result.push_back("cfg80211:cfg80211_new_sta");
      if (cfg80211.has_cfg80211_notify_new_peer_candidate() && cfg80211.cfg80211_notify_new_peer_candidate())
        result.push_back("cfg80211:cfg80211_notify_new_peer_candidate");
      if (cfg80211.has_cfg80211_pmksa_candidate_notify() && cfg80211.cfg80211_pmksa_candidate_notify())
        result.push_back("cfg80211:cfg80211_pmksa_candidate_notify");
      if (cfg80211.has_cfg80211_probe_status() && cfg80211.cfg80211_probe_status())
        result.push_back("cfg80211:cfg80211_probe_status");
      if (cfg80211.has_cfg80211_radar_event() && cfg80211.cfg80211_radar_event())
        result.push_back("cfg80211:cfg80211_radar_event");
      if (cfg80211.has_cfg80211_ready_on_channel() && cfg80211.cfg80211_ready_on_channel())
        result.push_back("cfg80211:cfg80211_ready_on_channel");
      if (cfg80211.has_cfg80211_ready_on_channel_expired() && cfg80211.cfg80211_ready_on_channel_expired())
        result.push_back("cfg80211:cfg80211_ready_on_channel_expired");
      if (cfg80211.has_cfg80211_reg_can_beacon() && cfg80211.cfg80211_reg_can_beacon())
        result.push_back("cfg80211:cfg80211_reg_can_beacon");
      if (cfg80211.has_cfg80211_report_obss_beacon() && cfg80211.cfg80211_report_obss_beacon())
        result.push_back("cfg80211:cfg80211_report_obss_beacon");
      if (cfg80211.has_cfg80211_report_wowlan_wakeup() && cfg80211.cfg80211_report_wowlan_wakeup())
        result.push_back("cfg80211:cfg80211_report_wowlan_wakeup");
      if (cfg80211.has_cfg80211_return_bool() && cfg80211.cfg80211_return_bool())
        result.push_back("cfg80211:cfg80211_return_bool");
      if (cfg80211.has_cfg80211_return_bss() && cfg80211.cfg80211_return_bss())
        result.push_back("cfg80211:cfg80211_return_bss");
      if (cfg80211.has_cfg80211_return_u32() && cfg80211.cfg80211_return_u32())
        result.push_back("cfg80211:cfg80211_return_u32");
      if (cfg80211.has_cfg80211_return_uint() && cfg80211.cfg80211_return_uint())
        result.push_back("cfg80211:cfg80211_return_uint");
      if (cfg80211.has_cfg80211_rx_mgmt() && cfg80211.cfg80211_rx_mgmt())
        result.push_back("cfg80211:cfg80211_rx_mgmt");
      if (cfg80211.has_cfg80211_rx_mlme_mgmt() && cfg80211.cfg80211_rx_mlme_mgmt())
        result.push_back("cfg80211:cfg80211_rx_mlme_mgmt");
      if (cfg80211.has_cfg80211_rx_spurious_frame() && cfg80211.cfg80211_rx_spurious_frame())
        result.push_back("cfg80211:cfg80211_rx_spurious_frame");
      if (cfg80211.has_cfg80211_rx_unexpected_4addr_frame() && cfg80211.cfg80211_rx_unexpected_4addr_frame())
        result.push_back("cfg80211:cfg80211_rx_unexpected_4addr_frame");
      if (cfg80211.has_cfg80211_rx_unprot_mlme_mgmt() && cfg80211.cfg80211_rx_unprot_mlme_mgmt())
        result.push_back("cfg80211:cfg80211_rx_unprot_mlme_mgmt");
      if (cfg80211.has_cfg80211_scan_done() && cfg80211.cfg80211_scan_done())
        result.push_back("cfg80211:cfg80211_scan_done");
      if (cfg80211.has_cfg80211_sched_scan_results() && cfg80211.cfg80211_sched_scan_results())
        result.push_back("cfg80211:cfg80211_sched_scan_results");
      if (cfg80211.has_cfg80211_sched_scan_stopped() && cfg80211.cfg80211_sched_scan_stopped())
        result.push_back("cfg80211:cfg80211_sched_scan_stopped");
      if (cfg80211.has_cfg80211_send_assoc_timeout() && cfg80211.cfg80211_send_assoc_timeout())
        result.push_back("cfg80211:cfg80211_send_assoc_timeout");
      if (cfg80211.has_cfg80211_send_auth_timeout() && cfg80211.cfg80211_send_auth_timeout())
        result.push_back("cfg80211:cfg80211_send_auth_timeout");
      if (cfg80211.has_cfg80211_send_rx_assoc() && cfg80211.cfg80211_send_rx_assoc())
        result.push_back("cfg80211:cfg80211_send_rx_assoc");
      if (cfg80211.has_cfg80211_send_rx_auth() && cfg80211.cfg80211_send_rx_auth())
        result.push_back("cfg80211:cfg80211_send_rx_auth");
      if (cfg80211.has_cfg80211_stop_iface() && cfg80211.cfg80211_stop_iface())
        result.push_back("cfg80211:cfg80211_stop_iface");
      if (cfg80211.has_cfg80211_tdls_oper_request() && cfg80211.cfg80211_tdls_oper_request())
        result.push_back("cfg80211:cfg80211_tdls_oper_request");
      if (cfg80211.has_cfg80211_tx_mlme_mgmt() && cfg80211.cfg80211_tx_mlme_mgmt())
        result.push_back("cfg80211:cfg80211_tx_mlme_mgmt");
      if (cfg80211.has_rdev_abort_scan() && cfg80211.rdev_abort_scan())
        result.push_back("cfg80211:rdev_abort_scan");
      if (cfg80211.has_rdev_add_key() && cfg80211.rdev_add_key())
        result.push_back("cfg80211:rdev_add_key");
      if (cfg80211.has_rdev_add_mpath() && cfg80211.rdev_add_mpath())
        result.push_back("cfg80211:rdev_add_mpath");
      if (cfg80211.has_rdev_add_station() && cfg80211.rdev_add_station())
        result.push_back("cfg80211:rdev_add_station");
      if (cfg80211.has_rdev_add_tx_ts() && cfg80211.rdev_add_tx_ts())
        result.push_back("cfg80211:rdev_add_tx_ts");
      if (cfg80211.has_rdev_add_virtual_intf() && cfg80211.rdev_add_virtual_intf())
        result.push_back("cfg80211:rdev_add_virtual_intf");
      if (cfg80211.has_rdev_assoc() && cfg80211.rdev_assoc())
        result.push_back("cfg80211:rdev_assoc");
      if (cfg80211.has_rdev_auth() && cfg80211.rdev_auth())
        result.push_back("cfg80211:rdev_auth");
      if (cfg80211.has_rdev_cancel_remain_on_channel() && cfg80211.rdev_cancel_remain_on_channel())
        result.push_back("cfg80211:rdev_cancel_remain_on_channel");
      if (cfg80211.has_rdev_change_beacon() && cfg80211.rdev_change_beacon())
        result.push_back("cfg80211:rdev_change_beacon");
      if (cfg80211.has_rdev_change_bss() && cfg80211.rdev_change_bss())
        result.push_back("cfg80211:rdev_change_bss");
      if (cfg80211.has_rdev_change_mpath() && cfg80211.rdev_change_mpath())
        result.push_back("cfg80211:rdev_change_mpath");
      if (cfg80211.has_rdev_change_station() && cfg80211.rdev_change_station())
        result.push_back("cfg80211:rdev_change_station");
      if (cfg80211.has_rdev_change_virtual_intf() && cfg80211.rdev_change_virtual_intf())
        result.push_back("cfg80211:rdev_change_virtual_intf");
      if (cfg80211.has_rdev_channel_switch() && cfg80211.rdev_channel_switch())
        result.push_back("cfg80211:rdev_channel_switch");
      if (cfg80211.has_rdev_connect() && cfg80211.rdev_connect())
        result.push_back("cfg80211:rdev_connect");
      if (cfg80211.has_rdev_crit_proto_start() && cfg80211.rdev_crit_proto_start())
        result.push_back("cfg80211:rdev_crit_proto_start");
      if (cfg80211.has_rdev_crit_proto_stop() && cfg80211.rdev_crit_proto_stop())
        result.push_back("cfg80211:rdev_crit_proto_stop");
      if (cfg80211.has_rdev_deauth() && cfg80211.rdev_deauth())
        result.push_back("cfg80211:rdev_deauth");
      if (cfg80211.has_rdev_del_key() && cfg80211.rdev_del_key())
        result.push_back("cfg80211:rdev_del_key");
      if (cfg80211.has_rdev_del_mpath() && cfg80211.rdev_del_mpath())
        result.push_back("cfg80211:rdev_del_mpath");
      if (cfg80211.has_rdev_del_pmksa() && cfg80211.rdev_del_pmksa())
        result.push_back("cfg80211:rdev_del_pmksa");
      if (cfg80211.has_rdev_del_station() && cfg80211.rdev_del_station())
        result.push_back("cfg80211:rdev_del_station");
      if (cfg80211.has_rdev_del_tx_ts() && cfg80211.rdev_del_tx_ts())
        result.push_back("cfg80211:rdev_del_tx_ts");
      if (cfg80211.has_rdev_del_virtual_intf() && cfg80211.rdev_del_virtual_intf())
        result.push_back("cfg80211:rdev_del_virtual_intf");
      if (cfg80211.has_rdev_disassoc() && cfg80211.rdev_disassoc())
        result.push_back("cfg80211:rdev_disassoc");
      if (cfg80211.has_rdev_disconnect() && cfg80211.rdev_disconnect())
        result.push_back("cfg80211:rdev_disconnect");
      if (cfg80211.has_rdev_dump_mpath() && cfg80211.rdev_dump_mpath())
        result.push_back("cfg80211:rdev_dump_mpath");
      if (cfg80211.has_rdev_dump_station() && cfg80211.rdev_dump_station())
        result.push_back("cfg80211:rdev_dump_station");
      if (cfg80211.has_rdev_dump_survey() && cfg80211.rdev_dump_survey())
        result.push_back("cfg80211:rdev_dump_survey");
      if (cfg80211.has_rdev_flush_pmksa() && cfg80211.rdev_flush_pmksa())
        result.push_back("cfg80211:rdev_flush_pmksa");
      if (cfg80211.has_rdev_get_antenna() && cfg80211.rdev_get_antenna())
        result.push_back("cfg80211:rdev_get_antenna");
      if (cfg80211.has_rdev_get_channel() && cfg80211.rdev_get_channel())
        result.push_back("cfg80211:rdev_get_channel");
      if (cfg80211.has_rdev_get_key() && cfg80211.rdev_get_key())
        result.push_back("cfg80211:rdev_get_key");
      if (cfg80211.has_rdev_get_mesh_config() && cfg80211.rdev_get_mesh_config())
        result.push_back("cfg80211:rdev_get_mesh_config");
      if (cfg80211.has_rdev_get_mpath() && cfg80211.rdev_get_mpath())
        result.push_back("cfg80211:rdev_get_mpath");
      if (cfg80211.has_rdev_get_station() && cfg80211.rdev_get_station())
        result.push_back("cfg80211:rdev_get_station");
      if (cfg80211.has_rdev_get_tx_power() && cfg80211.rdev_get_tx_power())
        result.push_back("cfg80211:rdev_get_tx_power");
      if (cfg80211.has_rdev_join_ibss() && cfg80211.rdev_join_ibss())
        result.push_back("cfg80211:rdev_join_ibss");
      if (cfg80211.has_rdev_join_mesh() && cfg80211.rdev_join_mesh())
        result.push_back("cfg80211:rdev_join_mesh");
      if (cfg80211.has_rdev_leave_ibss() && cfg80211.rdev_leave_ibss())
        result.push_back("cfg80211:rdev_leave_ibss");
      if (cfg80211.has_rdev_leave_mesh() && cfg80211.rdev_leave_mesh())
        result.push_back("cfg80211:rdev_leave_mesh");
      if (cfg80211.has_rdev_libertas_set_mesh_channel() && cfg80211.rdev_libertas_set_mesh_channel())
        result.push_back("cfg80211:rdev_libertas_set_mesh_channel");
      if (cfg80211.has_rdev_mgmt_frame_register() && cfg80211.rdev_mgmt_frame_register())
        result.push_back("cfg80211:rdev_mgmt_frame_register");
      if (cfg80211.has_rdev_mgmt_tx() && cfg80211.rdev_mgmt_tx())
        result.push_back("cfg80211:rdev_mgmt_tx");
      if (cfg80211.has_rdev_mgmt_tx_cancel_wait() && cfg80211.rdev_mgmt_tx_cancel_wait())
        result.push_back("cfg80211:rdev_mgmt_tx_cancel_wait");
      if (cfg80211.has_rdev_probe_client() && cfg80211.rdev_probe_client())
        result.push_back("cfg80211:rdev_probe_client");
      if (cfg80211.has_rdev_remain_on_channel() && cfg80211.rdev_remain_on_channel())
        result.push_back("cfg80211:rdev_remain_on_channel");
      if (cfg80211.has_rdev_resume() && cfg80211.rdev_resume())
        result.push_back("cfg80211:rdev_resume");
      if (cfg80211.has_rdev_return_chandef() && cfg80211.rdev_return_chandef())
        result.push_back("cfg80211:rdev_return_chandef");
      if (cfg80211.has_rdev_return_int() && cfg80211.rdev_return_int())
        result.push_back("cfg80211:rdev_return_int");
      if (cfg80211.has_rdev_return_int_cookie() && cfg80211.rdev_return_int_cookie())
        result.push_back("cfg80211:rdev_return_int_cookie");
      if (cfg80211.has_rdev_return_int_int() && cfg80211.rdev_return_int_int())
        result.push_back("cfg80211:rdev_return_int_int");
      if (cfg80211.has_rdev_return_int_mesh_config() && cfg80211.rdev_return_int_mesh_config())
        result.push_back("cfg80211:rdev_return_int_mesh_config");
      if (cfg80211.has_rdev_return_int_mpath_info() && cfg80211.rdev_return_int_mpath_info())
        result.push_back("cfg80211:rdev_return_int_mpath_info");
      if (cfg80211.has_rdev_return_int_station_info() && cfg80211.rdev_return_int_station_info())
        result.push_back("cfg80211:rdev_return_int_station_info");
      if (cfg80211.has_rdev_return_int_survey_info() && cfg80211.rdev_return_int_survey_info())
        result.push_back("cfg80211:rdev_return_int_survey_info");
      if (cfg80211.has_rdev_return_int_tx_rx() && cfg80211.rdev_return_int_tx_rx())
        result.push_back("cfg80211:rdev_return_int_tx_rx");
      if (cfg80211.has_rdev_return_void() && cfg80211.rdev_return_void())
        result.push_back("cfg80211:rdev_return_void");
      if (cfg80211.has_rdev_return_void_tx_rx() && cfg80211.rdev_return_void_tx_rx())
        result.push_back("cfg80211:rdev_return_void_tx_rx");
      if (cfg80211.has_rdev_return_wdev() && cfg80211.rdev_return_wdev())
        result.push_back("cfg80211:rdev_return_wdev");
      if (cfg80211.has_rdev_rfkill_poll() && cfg80211.rdev_rfkill_poll())
        result.push_back("cfg80211:rdev_rfkill_poll");
      if (cfg80211.has_rdev_scan() && cfg80211.rdev_scan())
        result.push_back("cfg80211:rdev_scan");
      if (cfg80211.has_rdev_sched_scan_start() && cfg80211.rdev_sched_scan_start())
        result.push_back("cfg80211:rdev_sched_scan_start");
      if (cfg80211.has_rdev_sched_scan_stop() && cfg80211.rdev_sched_scan_stop())
        result.push_back("cfg80211:rdev_sched_scan_stop");
      if (cfg80211.has_rdev_set_antenna() && cfg80211.rdev_set_antenna())
        result.push_back("cfg80211:rdev_set_antenna");
      if (cfg80211.has_rdev_set_ap_chanwidth() && cfg80211.rdev_set_ap_chanwidth())
        result.push_back("cfg80211:rdev_set_ap_chanwidth");
      if (cfg80211.has_rdev_set_bitrate_mask() && cfg80211.rdev_set_bitrate_mask())
        result.push_back("cfg80211:rdev_set_bitrate_mask");
      if (cfg80211.has_rdev_set_cqm_rssi_config() && cfg80211.rdev_set_cqm_rssi_config())
        result.push_back("cfg80211:rdev_set_cqm_rssi_config");
      if (cfg80211.has_rdev_set_cqm_txe_config() && cfg80211.rdev_set_cqm_txe_config())
        result.push_back("cfg80211:rdev_set_cqm_txe_config");
      if (cfg80211.has_rdev_set_default_key() && cfg80211.rdev_set_default_key())
        result.push_back("cfg80211:rdev_set_default_key");
      if (cfg80211.has_rdev_set_default_mgmt_key() && cfg80211.rdev_set_default_mgmt_key())
        result.push_back("cfg80211:rdev_set_default_mgmt_key");
      if (cfg80211.has_rdev_set_mac_acl() && cfg80211.rdev_set_mac_acl())
        result.push_back("cfg80211:rdev_set_mac_acl");
      if (cfg80211.has_rdev_set_monitor_channel() && cfg80211.rdev_set_monitor_channel())
        result.push_back("cfg80211:rdev_set_monitor_channel");
      if (cfg80211.has_rdev_set_noack_map() && cfg80211.rdev_set_noack_map())
        result.push_back("cfg80211:rdev_set_noack_map");
      if (cfg80211.has_rdev_set_pmksa() && cfg80211.rdev_set_pmksa())
        result.push_back("cfg80211:rdev_set_pmksa");
      if (cfg80211.has_rdev_set_power_mgmt() && cfg80211.rdev_set_power_mgmt())
        result.push_back("cfg80211:rdev_set_power_mgmt");
      if (cfg80211.has_rdev_set_qos_map() && cfg80211.rdev_set_qos_map())
        result.push_back("cfg80211:rdev_set_qos_map");
      if (cfg80211.has_rdev_set_rekey_data() && cfg80211.rdev_set_rekey_data())
        result.push_back("cfg80211:rdev_set_rekey_data");
      if (cfg80211.has_rdev_set_tx_power() && cfg80211.rdev_set_tx_power())
        result.push_back("cfg80211:rdev_set_tx_power");
      if (cfg80211.has_rdev_set_txq_params() && cfg80211.rdev_set_txq_params())
        result.push_back("cfg80211:rdev_set_txq_params");
      if (cfg80211.has_rdev_set_wakeup() && cfg80211.rdev_set_wakeup())
        result.push_back("cfg80211:rdev_set_wakeup");
      if (cfg80211.has_rdev_set_wds_peer() && cfg80211.rdev_set_wds_peer())
        result.push_back("cfg80211:rdev_set_wds_peer");
      if (cfg80211.has_rdev_set_wiphy_params() && cfg80211.rdev_set_wiphy_params())
        result.push_back("cfg80211:rdev_set_wiphy_params");
      if (cfg80211.has_rdev_start_ap() && cfg80211.rdev_start_ap())
        result.push_back("cfg80211:rdev_start_ap");
      if (cfg80211.has_rdev_start_p2p_device() && cfg80211.rdev_start_p2p_device())
        result.push_back("cfg80211:rdev_start_p2p_device");
      if (cfg80211.has_rdev_stop_ap() && cfg80211.rdev_stop_ap())
        result.push_back("cfg80211:rdev_stop_ap");
      if (cfg80211.has_rdev_stop_p2p_device() && cfg80211.rdev_stop_p2p_device())
        result.push_back("cfg80211:rdev_stop_p2p_device");
      if (cfg80211.has_rdev_suspend() && cfg80211.rdev_suspend())
        result.push_back("cfg80211:rdev_suspend");
      if (cfg80211.has_rdev_tdls_mgmt() && cfg80211.rdev_tdls_mgmt())
        result.push_back("cfg80211:rdev_tdls_mgmt");
      if (cfg80211.has_rdev_tdls_oper() && cfg80211.rdev_tdls_oper())
        result.push_back("cfg80211:rdev_tdls_oper");
      if (cfg80211.has_rdev_testmode_cmd() && cfg80211.rdev_testmode_cmd())
        result.push_back("cfg80211:rdev_testmode_cmd");
      if (cfg80211.has_rdev_testmode_dump() && cfg80211.rdev_testmode_dump())
        result.push_back("cfg80211:rdev_testmode_dump");
      if (cfg80211.has_rdev_update_connect_params() && cfg80211.rdev_update_connect_params())
        result.push_back("cfg80211:rdev_update_connect_params");
      if (cfg80211.has_rdev_update_ft_ies() && cfg80211.rdev_update_ft_ies())
        result.push_back("cfg80211:rdev_update_ft_ies");
      if (cfg80211.has_rdev_update_mesh_config() && cfg80211.rdev_update_mesh_config())
        result.push_back("cfg80211:rdev_update_mesh_config");
    }

    if (tracepoints.has_cma()) {
      const auto& cma = tracepoints.cma();

      if (cma.has_cma_alloc() && cma.cma_alloc())
        result.push_back("cma:cma_alloc");
      if (cma.has_cma_alloc_busy_retry() && cma.cma_alloc_busy_retry())
        result.push_back("cma:cma_alloc_busy_retry");
      if (cma.has_cma_alloc_start() && cma.cma_alloc_start())
        result.push_back("cma:cma_alloc_start");
      if (cma.has_cma_release() && cma.cma_release())
        result.push_back("cma:cma_release");
    }

    if (tracepoints.has_compaction()) {
      const auto& compaction = tracepoints.compaction();

      if (compaction.has_mm_compaction_begin() && compaction.mm_compaction_begin())
        result.push_back("compaction:mm_compaction_begin");
      if (compaction.has_mm_compaction_end() && compaction.mm_compaction_end())
        result.push_back("compaction:mm_compaction_end");
      if (compaction.has_mm_compaction_isolate_freepages() && compaction.mm_compaction_isolate_freepages())
        result.push_back("compaction:mm_compaction_isolate_freepages");
      if (compaction.has_mm_compaction_isolate_migratepages() && compaction.mm_compaction_isolate_migratepages())
        result.push_back("compaction:mm_compaction_isolate_migratepages");
      if (compaction.has_mm_compaction_migratepages() && compaction.mm_compaction_migratepages())
        result.push_back("compaction:mm_compaction_migratepages");
    }

    if (tracepoints.has_cpufreq_interactive()) {
      const auto& cpufreq_interactive = tracepoints.cpufreq_interactive();

      if (cpufreq_interactive.has_cpufreq_interactive_already() && cpufreq_interactive.cpufreq_interactive_already())
        result.push_back("cpufreq_interactive:cpufreq_interactive_already");
      if (cpufreq_interactive.has_cpufreq_interactive_boost() && cpufreq_interactive.cpufreq_interactive_boost())
        result.push_back("cpufreq_interactive:cpufreq_interactive_boost");
      if (cpufreq_interactive.has_cpufreq_interactive_cpuload() && cpufreq_interactive.cpufreq_interactive_cpuload())
        result.push_back("cpufreq_interactive:cpufreq_interactive_cpuload");
      if (cpufreq_interactive.has_cpufreq_interactive_load_change() && cpufreq_interactive.cpufreq_interactive_load_change())
        result.push_back("cpufreq_interactive:cpufreq_interactive_load_change");
      if (cpufreq_interactive.has_cpufreq_interactive_notyet() && cpufreq_interactive.cpufreq_interactive_notyet())
        result.push_back("cpufreq_interactive:cpufreq_interactive_notyet");
      if (cpufreq_interactive.has_cpufreq_interactive_setspeed() && cpufreq_interactive.cpufreq_interactive_setspeed())
        result.push_back("cpufreq_interactive:cpufreq_interactive_setspeed");
      if (cpufreq_interactive.has_cpufreq_interactive_target() && cpufreq_interactive.cpufreq_interactive_target())
        result.push_back("cpufreq_interactive:cpufreq_interactive_target");
      if (cpufreq_interactive.has_cpufreq_interactive_unboost() && cpufreq_interactive.cpufreq_interactive_unboost())
        result.push_back("cpufreq_interactive:cpufreq_interactive_unboost");
    }

    if (tracepoints.has_cpufreq_sched()) {
      const auto& cpufreq_sched = tracepoints.cpufreq_sched();

      if (cpufreq_sched.has_cpufreq_sched_request_opp() && cpufreq_sched.cpufreq_sched_request_opp())
        result.push_back("cpufreq_sched:cpufreq_sched_request_opp");
      if (cpufreq_sched.has_cpufreq_sched_throttled() && cpufreq_sched.cpufreq_sched_throttled())
        result.push_back("cpufreq_sched:cpufreq_sched_throttled");
      if (cpufreq_sched.has_cpufreq_sched_update_capacity() && cpufreq_sched.cpufreq_sched_update_capacity())
        result.push_back("cpufreq_sched:cpufreq_sched_update_capacity");
    }

    if (tracepoints.has_devfreq()) {
      const auto& devfreq = tracepoints.devfreq();

      if (devfreq.has_devfreq_msg() && devfreq.devfreq_msg())
        result.push_back("devfreq:devfreq_msg");
    }

    if (tracepoints.has_dwc3()) {
      const auto& dwc3 = tracepoints.dwc3();

      if (dwc3.has_dwc3_alloc_request() && dwc3.dwc3_alloc_request())
        result.push_back("dwc3:dwc3_alloc_request");
      if (dwc3.has_dwc3_complete_trb() && dwc3.dwc3_complete_trb())
        result.push_back("dwc3:dwc3_complete_trb");
      if (dwc3.has_dwc3_ctrl_req() && dwc3.dwc3_ctrl_req())
        result.push_back("dwc3:dwc3_ctrl_req");
      if (dwc3.has_dwc3_ep0() && dwc3.dwc3_ep0())
        result.push_back("dwc3:dwc3_ep0");
      if (dwc3.has_dwc3_ep_dequeue() && dwc3.dwc3_ep_dequeue())
        result.push_back("dwc3:dwc3_ep_dequeue");
      if (dwc3.has_dwc3_ep_queue() && dwc3.dwc3_ep_queue())
        result.push_back("dwc3:dwc3_ep_queue");
      if (dwc3.has_dwc3_event() && dwc3.dwc3_event())
        result.push_back("dwc3:dwc3_event");
      if (dwc3.has_dwc3_free_request() && dwc3.dwc3_free_request())
        result.push_back("dwc3:dwc3_free_request");
      if (dwc3.has_dwc3_gadget_ep_cmd() && dwc3.dwc3_gadget_ep_cmd())
        result.push_back("dwc3:dwc3_gadget_ep_cmd");
      if (dwc3.has_dwc3_gadget_generic_cmd() && dwc3.dwc3_gadget_generic_cmd())
        result.push_back("dwc3:dwc3_gadget_generic_cmd");
      if (dwc3.has_dwc3_gadget_giveback() && dwc3.dwc3_gadget_giveback())
        result.push_back("dwc3:dwc3_gadget_giveback");
      if (dwc3.has_dwc3_prepare_trb() && dwc3.dwc3_prepare_trb())
        result.push_back("dwc3:dwc3_prepare_trb");
      if (dwc3.has_dwc3_readl() && dwc3.dwc3_readl())
        result.push_back("dwc3:dwc3_readl");
      if (dwc3.has_dwc3_writel() && dwc3.dwc3_writel())
        result.push_back("dwc3:dwc3_writel");
    }

    if (tracepoints.has_emulation()) {
      const auto& emulation = tracepoints.emulation();

      if (emulation.has_instruction_emulation() && emulation.instruction_emulation())
        result.push_back("emulation:instruction_emulation");
    }

    if (tracepoints.has_exception()) {
      const auto& exception = tracepoints.exception();

      if (exception.has_kernel_panic() && exception.kernel_panic())
        result.push_back("exception:kernel_panic");
      if (exception.has_kernel_panic_late() && exception.kernel_panic_late())
        result.push_back("exception:kernel_panic_late");
      if (exception.has_undef_instr() && exception.undef_instr())
        result.push_back("exception:undef_instr");
      if (exception.has_unhandled_abort() && exception.unhandled_abort())
        result.push_back("exception:unhandled_abort");
      if (exception.has_user_fault() && exception.user_fault())
        result.push_back("exception:user_fault");
    }

    if (tracepoints.has_ext4()) {
      const auto& ext4 = tracepoints.ext4();

      if (ext4.has_ext4_alloc_da_blocks() && ext4.ext4_alloc_da_blocks())
        result.push_back("ext4:ext4_alloc_da_blocks");
      if (ext4.has_ext4_allocate_blocks() && ext4.ext4_allocate_blocks())
        result.push_back("ext4:ext4_allocate_blocks");
      if (ext4.has_ext4_allocate_inode() && ext4.ext4_allocate_inode())
        result.push_back("ext4:ext4_allocate_inode");
      if (ext4.has_ext4_begin_ordered_truncate() && ext4.ext4_begin_ordered_truncate())
        result.push_back("ext4:ext4_begin_ordered_truncate");
      if (ext4.has_ext4_collapse_range() && ext4.ext4_collapse_range())
        result.push_back("ext4:ext4_collapse_range");
      if (ext4.has_ext4_da_release_space() && ext4.ext4_da_release_space())
        result.push_back("ext4:ext4_da_release_space");
      if (ext4.has_ext4_da_reserve_space() && ext4.ext4_da_reserve_space())
        result.push_back("ext4:ext4_da_reserve_space");
      if (ext4.has_ext4_da_update_reserve_space() && ext4.ext4_da_update_reserve_space())
        result.push_back("ext4:ext4_da_update_reserve_space");
      if (ext4.has_ext4_da_write_begin() && ext4.ext4_da_write_begin())
        result.push_back("ext4:ext4_da_write_begin");
      if (ext4.has_ext4_da_write_end() && ext4.ext4_da_write_end())
        result.push_back("ext4:ext4_da_write_end");
      if (ext4.has_ext4_da_write_pages() && ext4.ext4_da_write_pages())
        result.push_back("ext4:ext4_da_write_pages");
      if (ext4.has_ext4_da_write_pages_extent() && ext4.ext4_da_write_pages_extent())
        result.push_back("ext4:ext4_da_write_pages_extent");
      if (ext4.has_ext4_direct_io_enter() && ext4.ext4_direct_io_enter())
        result.push_back("ext4:ext4_direct_IO_enter");
      if (ext4.has_ext4_direct_io_exit() && ext4.ext4_direct_io_exit())
        result.push_back("ext4:ext4_direct_IO_exit");
      if (ext4.has_ext4_discard_blocks() && ext4.ext4_discard_blocks())
        result.push_back("ext4:ext4_discard_blocks");
      if (ext4.has_ext4_discard_preallocations() && ext4.ext4_discard_preallocations())
        result.push_back("ext4:ext4_discard_preallocations");
      if (ext4.has_ext4_drop_inode() && ext4.ext4_drop_inode())
        result.push_back("ext4:ext4_drop_inode");
      if (ext4.has_ext4_es_cache_extent() && ext4.ext4_es_cache_extent())
        result.push_back("ext4:ext4_es_cache_extent");
      if (ext4.has_ext4_es_find_delayed_extent_range_enter() && ext4.ext4_es_find_delayed_extent_range_enter())
        result.push_back("ext4:ext4_es_find_delayed_extent_range_enter");
      if (ext4.has_ext4_es_find_delayed_extent_range_exit() && ext4.ext4_es_find_delayed_extent_range_exit())
        result.push_back("ext4:ext4_es_find_delayed_extent_range_exit");
      if (ext4.has_ext4_es_insert_extent() && ext4.ext4_es_insert_extent())
        result.push_back("ext4:ext4_es_insert_extent");
      if (ext4.has_ext4_es_lookup_extent_enter() && ext4.ext4_es_lookup_extent_enter())
        result.push_back("ext4:ext4_es_lookup_extent_enter");
      if (ext4.has_ext4_es_lookup_extent_exit() && ext4.ext4_es_lookup_extent_exit())
        result.push_back("ext4:ext4_es_lookup_extent_exit");
      if (ext4.has_ext4_es_remove_extent() && ext4.ext4_es_remove_extent())
        result.push_back("ext4:ext4_es_remove_extent");
      if (ext4.has_ext4_es_shrink() && ext4.ext4_es_shrink())
        result.push_back("ext4:ext4_es_shrink");
      if (ext4.has_ext4_es_shrink_count() && ext4.ext4_es_shrink_count())
        result.push_back("ext4:ext4_es_shrink_count");
      if (ext4.has_ext4_es_shrink_scan_enter() && ext4.ext4_es_shrink_scan_enter())
        result.push_back("ext4:ext4_es_shrink_scan_enter");
      if (ext4.has_ext4_es_shrink_scan_exit() && ext4.ext4_es_shrink_scan_exit())
        result.push_back("ext4:ext4_es_shrink_scan_exit");
      if (ext4.has_ext4_evict_inode() && ext4.ext4_evict_inode())
        result.push_back("ext4:ext4_evict_inode");
      if (ext4.has_ext4_ext_convert_to_initialized_enter() && ext4.ext4_ext_convert_to_initialized_enter())
        result.push_back("ext4:ext4_ext_convert_to_initialized_enter");
      if (ext4.has_ext4_ext_convert_to_initialized_fastpath() && ext4.ext4_ext_convert_to_initialized_fastpath())
        result.push_back("ext4:ext4_ext_convert_to_initialized_fastpath");
      if (ext4.has_ext4_ext_handle_unwritten_extents() && ext4.ext4_ext_handle_unwritten_extents())
        result.push_back("ext4:ext4_ext_handle_unwritten_extents");
      if (ext4.has_ext4_ext_in_cache() && ext4.ext4_ext_in_cache())
        result.push_back("ext4:ext4_ext_in_cache");
      if (ext4.has_ext4_ext_load_extent() && ext4.ext4_ext_load_extent())
        result.push_back("ext4:ext4_ext_load_extent");
      if (ext4.has_ext4_ext_map_blocks_enter() && ext4.ext4_ext_map_blocks_enter())
        result.push_back("ext4:ext4_ext_map_blocks_enter");
      if (ext4.has_ext4_ext_map_blocks_exit() && ext4.ext4_ext_map_blocks_exit())
        result.push_back("ext4:ext4_ext_map_blocks_exit");
      if (ext4.has_ext4_ext_put_in_cache() && ext4.ext4_ext_put_in_cache())
        result.push_back("ext4:ext4_ext_put_in_cache");
      if (ext4.has_ext4_ext_remove_space() && ext4.ext4_ext_remove_space())
        result.push_back("ext4:ext4_ext_remove_space");
      if (ext4.has_ext4_ext_remove_space_done() && ext4.ext4_ext_remove_space_done())
        result.push_back("ext4:ext4_ext_remove_space_done");
      if (ext4.has_ext4_ext_rm_idx() && ext4.ext4_ext_rm_idx())
        result.push_back("ext4:ext4_ext_rm_idx");
      if (ext4.has_ext4_ext_rm_leaf() && ext4.ext4_ext_rm_leaf())
        result.push_back("ext4:ext4_ext_rm_leaf");
      if (ext4.has_ext4_ext_show_extent() && ext4.ext4_ext_show_extent())
        result.push_back("ext4:ext4_ext_show_extent");
      if (ext4.has_ext4_fallocate_enter() && ext4.ext4_fallocate_enter())
        result.push_back("ext4:ext4_fallocate_enter");
      if (ext4.has_ext4_fallocate_exit() && ext4.ext4_fallocate_exit())
        result.push_back("ext4:ext4_fallocate_exit");
      if (ext4.has_ext4_find_delalloc_range() && ext4.ext4_find_delalloc_range())
        result.push_back("ext4:ext4_find_delalloc_range");
      if (ext4.has_ext4_forget() && ext4.ext4_forget())
        result.push_back("ext4:ext4_forget");
      if (ext4.has_ext4_free_blocks() && ext4.ext4_free_blocks())
        result.push_back("ext4:ext4_free_blocks");
      if (ext4.has_ext4_free_inode() && ext4.ext4_free_inode())
        result.push_back("ext4:ext4_free_inode");
      if (ext4.has_ext4_get_implied_cluster_alloc_exit() && ext4.ext4_get_implied_cluster_alloc_exit())
        result.push_back("ext4:ext4_get_implied_cluster_alloc_exit");
      if (ext4.has_ext4_get_reserved_cluster_alloc() && ext4.ext4_get_reserved_cluster_alloc())
        result.push_back("ext4:ext4_get_reserved_cluster_alloc");
      if (ext4.has_ext4_ind_map_blocks_enter() && ext4.ext4_ind_map_blocks_enter())
        result.push_back("ext4:ext4_ind_map_blocks_enter");
      if (ext4.has_ext4_ind_map_blocks_exit() && ext4.ext4_ind_map_blocks_exit())
        result.push_back("ext4:ext4_ind_map_blocks_exit");
      if (ext4.has_ext4_invalidatepage() && ext4.ext4_invalidatepage())
        result.push_back("ext4:ext4_invalidatepage");
      if (ext4.has_ext4_journal_start() && ext4.ext4_journal_start())
        result.push_back("ext4:ext4_journal_start");
      if (ext4.has_ext4_journal_start_reserved() && ext4.ext4_journal_start_reserved())
        result.push_back("ext4:ext4_journal_start_reserved");
      if (ext4.has_ext4_journalled_invalidatepage() && ext4.ext4_journalled_invalidatepage())
        result.push_back("ext4:ext4_journalled_invalidatepage");
      if (ext4.has_ext4_journalled_write_end() && ext4.ext4_journalled_write_end())
        result.push_back("ext4:ext4_journalled_write_end");
      if (ext4.has_ext4_load_inode() && ext4.ext4_load_inode())
        result.push_back("ext4:ext4_load_inode");
      if (ext4.has_ext4_load_inode_bitmap() && ext4.ext4_load_inode_bitmap())
        result.push_back("ext4:ext4_load_inode_bitmap");
      if (ext4.has_ext4_mark_inode_dirty() && ext4.ext4_mark_inode_dirty())
        result.push_back("ext4:ext4_mark_inode_dirty");
      if (ext4.has_ext4_mb_bitmap_load() && ext4.ext4_mb_bitmap_load())
        result.push_back("ext4:ext4_mb_bitmap_load");
      if (ext4.has_ext4_mb_buddy_bitmap_load() && ext4.ext4_mb_buddy_bitmap_load())
        result.push_back("ext4:ext4_mb_buddy_bitmap_load");
      if (ext4.has_ext4_mb_discard_preallocations() && ext4.ext4_mb_discard_preallocations())
        result.push_back("ext4:ext4_mb_discard_preallocations");
      if (ext4.has_ext4_mb_new_group_pa() && ext4.ext4_mb_new_group_pa())
        result.push_back("ext4:ext4_mb_new_group_pa");
      if (ext4.has_ext4_mb_new_inode_pa() && ext4.ext4_mb_new_inode_pa())
        result.push_back("ext4:ext4_mb_new_inode_pa");
      if (ext4.has_ext4_mb_release_group_pa() && ext4.ext4_mb_release_group_pa())
        result.push_back("ext4:ext4_mb_release_group_pa");
      if (ext4.has_ext4_mb_release_inode_pa() && ext4.ext4_mb_release_inode_pa())
        result.push_back("ext4:ext4_mb_release_inode_pa");
      if (ext4.has_ext4_mballoc_alloc() && ext4.ext4_mballoc_alloc())
        result.push_back("ext4:ext4_mballoc_alloc");
      if (ext4.has_ext4_mballoc_discard() && ext4.ext4_mballoc_discard())
        result.push_back("ext4:ext4_mballoc_discard");
      if (ext4.has_ext4_mballoc_free() && ext4.ext4_mballoc_free())
        result.push_back("ext4:ext4_mballoc_free");
      if (ext4.has_ext4_mballoc_prealloc() && ext4.ext4_mballoc_prealloc())
        result.push_back("ext4:ext4_mballoc_prealloc");
      if (ext4.has_ext4_punch_hole() && ext4.ext4_punch_hole())
        result.push_back("ext4:ext4_punch_hole");
      if (ext4.has_ext4_read_block_bitmap_load() && ext4.ext4_read_block_bitmap_load())
        result.push_back("ext4:ext4_read_block_bitmap_load");
      if (ext4.has_ext4_readpage() && ext4.ext4_readpage())
        result.push_back("ext4:ext4_readpage");
      if (ext4.has_ext4_releasepage() && ext4.ext4_releasepage())
        result.push_back("ext4:ext4_releasepage");
      if (ext4.has_ext4_remove_blocks() && ext4.ext4_remove_blocks())
        result.push_back("ext4:ext4_remove_blocks");
      if (ext4.has_ext4_request_blocks() && ext4.ext4_request_blocks())
        result.push_back("ext4:ext4_request_blocks");
      if (ext4.has_ext4_request_inode() && ext4.ext4_request_inode())
        result.push_back("ext4:ext4_request_inode");
      if (ext4.has_ext4_sync_file_enter() && ext4.ext4_sync_file_enter())
        result.push_back("ext4:ext4_sync_file_enter");
      if (ext4.has_ext4_sync_file_exit() && ext4.ext4_sync_file_exit())
        result.push_back("ext4:ext4_sync_file_exit");
      if (ext4.has_ext4_sync_fs() && ext4.ext4_sync_fs())
        result.push_back("ext4:ext4_sync_fs");
      if (ext4.has_ext4_trim_all_free() && ext4.ext4_trim_all_free())
        result.push_back("ext4:ext4_trim_all_free");
      if (ext4.has_ext4_trim_extent() && ext4.ext4_trim_extent())
        result.push_back("ext4:ext4_trim_extent");
      if (ext4.has_ext4_truncate_enter() && ext4.ext4_truncate_enter())
        result.push_back("ext4:ext4_truncate_enter");
      if (ext4.has_ext4_truncate_exit() && ext4.ext4_truncate_exit())
        result.push_back("ext4:ext4_truncate_exit");
      if (ext4.has_ext4_unlink_enter() && ext4.ext4_unlink_enter())
        result.push_back("ext4:ext4_unlink_enter");
      if (ext4.has_ext4_unlink_exit() && ext4.ext4_unlink_exit())
        result.push_back("ext4:ext4_unlink_exit");
      if (ext4.has_ext4_write_begin() && ext4.ext4_write_begin())
        result.push_back("ext4:ext4_write_begin");
      if (ext4.has_ext4_write_end() && ext4.ext4_write_end())
        result.push_back("ext4:ext4_write_end");
      if (ext4.has_ext4_writepage() && ext4.ext4_writepage())
        result.push_back("ext4:ext4_writepage");
      if (ext4.has_ext4_writepages() && ext4.ext4_writepages())
        result.push_back("ext4:ext4_writepages");
      if (ext4.has_ext4_writepages_result() && ext4.ext4_writepages_result())
        result.push_back("ext4:ext4_writepages_result");
      if (ext4.has_ext4_zero_range() && ext4.ext4_zero_range())
        result.push_back("ext4:ext4_zero_range");
    }

    if (tracepoints.has_fence()) {
      const auto& fence = tracepoints.fence();

      if (fence.has_fence_annotate_wait_on() && fence.fence_annotate_wait_on())
        result.push_back("fence:fence_annotate_wait_on");
      if (fence.has_fence_destroy() && fence.fence_destroy())
        result.push_back("fence:fence_destroy");
      if (fence.has_fence_emit() && fence.fence_emit())
        result.push_back("fence:fence_emit");
      if (fence.has_fence_enable_signal() && fence.fence_enable_signal())
        result.push_back("fence:fence_enable_signal");
      if (fence.has_fence_init() && fence.fence_init())
        result.push_back("fence:fence_init");
      if (fence.has_fence_signaled() && fence.fence_signaled())
        result.push_back("fence:fence_signaled");
      if (fence.has_fence_wait_end() && fence.fence_wait_end())
        result.push_back("fence:fence_wait_end");
      if (fence.has_fence_wait_start() && fence.fence_wait_start())
        result.push_back("fence:fence_wait_start");
    }

    if (tracepoints.has_filelock()) {
      const auto& filelock = tracepoints.filelock();

      if (filelock.has_break_lease_block() && filelock.break_lease_block())
        result.push_back("filelock:break_lease_block");
      if (filelock.has_break_lease_noblock() && filelock.break_lease_noblock())
        result.push_back("filelock:break_lease_noblock");
      if (filelock.has_break_lease_unblock() && filelock.break_lease_unblock())
        result.push_back("filelock:break_lease_unblock");
      if (filelock.has_generic_add_lease() && filelock.generic_add_lease())
        result.push_back("filelock:generic_add_lease");
      if (filelock.has_generic_delete_lease() && filelock.generic_delete_lease())
        result.push_back("filelock:generic_delete_lease");
      if (filelock.has_time_out_leases() && filelock.time_out_leases())
        result.push_back("filelock:time_out_leases");
    }

    if (tracepoints.has_filemap()) {
      const auto& filemap = tracepoints.filemap();

      if (filemap.has_mm_filemap_add_to_page_cache() && filemap.mm_filemap_add_to_page_cache())
        result.push_back("filemap:mm_filemap_add_to_page_cache");
      if (filemap.has_mm_filemap_delete_from_page_cache() && filemap.mm_filemap_delete_from_page_cache())
        result.push_back("filemap:mm_filemap_delete_from_page_cache");
    }

    if (tracepoints.has_gpio()) {
      const auto& gpio = tracepoints.gpio();

      if (gpio.has_gpio_direction() && gpio.gpio_direction())
        result.push_back("gpio:gpio_direction");
      if (gpio.has_gpio_value() && gpio.gpio_value())
        result.push_back("gpio:gpio_value");
    }

    if (tracepoints.has_i2c()) {
      const auto& i2c = tracepoints.i2c();

      if (i2c.has_i2c_read() && i2c.i2c_read())
        result.push_back("i2c:i2c_read");
      if (i2c.has_i2c_reply() && i2c.i2c_reply())
        result.push_back("i2c:i2c_reply");
      if (i2c.has_i2c_result() && i2c.i2c_result())
        result.push_back("i2c:i2c_result");
      if (i2c.has_i2c_write() && i2c.i2c_write())
        result.push_back("i2c:i2c_write");
      if (i2c.has_smbus_read() && i2c.smbus_read())
        result.push_back("i2c:smbus_read");
      if (i2c.has_smbus_reply() && i2c.smbus_reply())
        result.push_back("i2c:smbus_reply");
      if (i2c.has_smbus_result() && i2c.smbus_result())
        result.push_back("i2c:smbus_result");
      if (i2c.has_smbus_write() && i2c.smbus_write())
        result.push_back("i2c:smbus_write");
    }

    if (tracepoints.has_iommu()) {
      const auto& iommu = tracepoints.iommu();

      if (iommu.has_add_device_to_group() && iommu.add_device_to_group())
        result.push_back("iommu:add_device_to_group");
      if (iommu.has_attach_device_to_domain() && iommu.attach_device_to_domain())
        result.push_back("iommu:attach_device_to_domain");
      if (iommu.has_detach_device_from_domain() && iommu.detach_device_from_domain())
        result.push_back("iommu:detach_device_from_domain");
      if (iommu.has_io_page_fault() && iommu.io_page_fault())
        result.push_back("iommu:io_page_fault");
      if (iommu.has_map() && iommu.map())
        result.push_back("iommu:map");
      if (iommu.has_map_end() && iommu.map_end())
        result.push_back("iommu:map_end");
      if (iommu.has_map_sg_end() && iommu.map_sg_end())
        result.push_back("iommu:map_sg_end");
      if (iommu.has_map_sg_start() && iommu.map_sg_start())
        result.push_back("iommu:map_sg_start");
      if (iommu.has_map_start() && iommu.map_start())
        result.push_back("iommu:map_start");
      if (iommu.has_remove_device_from_group() && iommu.remove_device_from_group())
        result.push_back("iommu:remove_device_from_group");
      if (iommu.has_unmap() && iommu.unmap())
        result.push_back("iommu:unmap");
      if (iommu.has_unmap_end() && iommu.unmap_end())
        result.push_back("iommu:unmap_end");
      if (iommu.has_unmap_start() && iommu.unmap_start())
        result.push_back("iommu:unmap_start");
    }

    if (tracepoints.has_ipa()) {
      const auto& ipa = tracepoints.ipa();

      if (ipa.has_idle_sleep_enter() && ipa.idle_sleep_enter())
        result.push_back("ipa:idle_sleep_enter");
      if (ipa.has_idle_sleep_exit() && ipa.idle_sleep_exit())
        result.push_back("ipa:idle_sleep_exit");
      if (ipa.has_intr_to_poll() && ipa.intr_to_poll())
        result.push_back("ipa:intr_to_poll");
      if (ipa.has_poll_to_intr() && ipa.poll_to_intr())
        result.push_back("ipa:poll_to_intr");
      if (ipa.has_rmnet_ipa_netifni() && ipa.rmnet_ipa_netifni())
        result.push_back("ipa:rmnet_ipa_netifni");
      if (ipa.has_rmnet_ipa_netifrx() && ipa.rmnet_ipa_netifrx())
        result.push_back("ipa:rmnet_ipa_netifrx");
    }

    if (tracepoints.has_ipi()) {
      const auto& ipi = tracepoints.ipi();

      if (ipi.has_ipi_entry() && ipi.ipi_entry())
        result.push_back("ipi:ipi_entry");
      if (ipi.has_ipi_exit() && ipi.ipi_exit())
        result.push_back("ipi:ipi_exit");
      if (ipi.has_ipi_raise() && ipi.ipi_raise())
        result.push_back("ipi:ipi_raise");
    }

    if (tracepoints.has_irq()) {
      const auto& irq = tracepoints.irq();

      if (irq.has_irq_handler_entry() && irq.irq_handler_entry())
        result.push_back("irq:irq_handler_entry");
      if (irq.has_irq_handler_exit() && irq.irq_handler_exit())
        result.push_back("irq:irq_handler_exit");
      if (irq.has_softirq_entry() && irq.softirq_entry())
        result.push_back("irq:softirq_entry");
      if (irq.has_softirq_exit() && irq.softirq_exit())
        result.push_back("irq:softirq_exit");
      if (irq.has_softirq_raise() && irq.softirq_raise())
        result.push_back("irq:softirq_raise");
    }

    if (tracepoints.has_jbd2()) {
      const auto& jbd2 = tracepoints.jbd2();

      if (jbd2.has_jbd2_checkpoint() && jbd2.jbd2_checkpoint())
        result.push_back("jbd2:jbd2_checkpoint");
      if (jbd2.has_jbd2_checkpoint_stats() && jbd2.jbd2_checkpoint_stats())
        result.push_back("jbd2:jbd2_checkpoint_stats");
      if (jbd2.has_jbd2_commit_flushing() && jbd2.jbd2_commit_flushing())
        result.push_back("jbd2:jbd2_commit_flushing");
      if (jbd2.has_jbd2_commit_locking() && jbd2.jbd2_commit_locking())
        result.push_back("jbd2:jbd2_commit_locking");
      if (jbd2.has_jbd2_commit_logging() && jbd2.jbd2_commit_logging())
        result.push_back("jbd2:jbd2_commit_logging");
      if (jbd2.has_jbd2_drop_transaction() && jbd2.jbd2_drop_transaction())
        result.push_back("jbd2:jbd2_drop_transaction");
      if (jbd2.has_jbd2_end_commit() && jbd2.jbd2_end_commit())
        result.push_back("jbd2:jbd2_end_commit");
      if (jbd2.has_jbd2_handle_extend() && jbd2.jbd2_handle_extend())
        result.push_back("jbd2:jbd2_handle_extend");
      if (jbd2.has_jbd2_handle_start() && jbd2.jbd2_handle_start())
        result.push_back("jbd2:jbd2_handle_start");
      if (jbd2.has_jbd2_handle_stats() && jbd2.jbd2_handle_stats())
        result.push_back("jbd2:jbd2_handle_stats");
      if (jbd2.has_jbd2_lock_buffer_stall() && jbd2.jbd2_lock_buffer_stall())
        result.push_back("jbd2:jbd2_lock_buffer_stall");
      if (jbd2.has_jbd2_run_stats() && jbd2.jbd2_run_stats())
        result.push_back("jbd2:jbd2_run_stats");
      if (jbd2.has_jbd2_start_commit() && jbd2.jbd2_start_commit())
        result.push_back("jbd2:jbd2_start_commit");
      if (jbd2.has_jbd2_submit_inode_data() && jbd2.jbd2_submit_inode_data())
        result.push_back("jbd2:jbd2_submit_inode_data");
      if (jbd2.has_jbd2_update_log_tail() && jbd2.jbd2_update_log_tail())
        result.push_back("jbd2:jbd2_update_log_tail");
      if (jbd2.has_jbd2_write_superblock() && jbd2.jbd2_write_superblock())
        result.push_back("jbd2:jbd2_write_superblock");
    }

    if (tracepoints.has_kgsl()) {
      const auto& kgsl = tracepoints.kgsl();

      if (kgsl.has_adreno_cmdbatch_fault() && kgsl.adreno_cmdbatch_fault())
        result.push_back("kgsl:adreno_cmdbatch_fault");
      if (kgsl.has_adreno_cmdbatch_queued() && kgsl.adreno_cmdbatch_queued())
        result.push_back("kgsl:adreno_cmdbatch_queued");
      if (kgsl.has_adreno_cmdbatch_recovery() && kgsl.adreno_cmdbatch_recovery())
        result.push_back("kgsl:adreno_cmdbatch_recovery");
      if (kgsl.has_adreno_cmdbatch_retired() && kgsl.adreno_cmdbatch_retired())
        result.push_back("kgsl:adreno_cmdbatch_retired");
      if (kgsl.has_adreno_cmdbatch_submitted() && kgsl.adreno_cmdbatch_submitted())
        result.push_back("kgsl:adreno_cmdbatch_submitted");
      if (kgsl.has_adreno_cmdbatch_sync() && kgsl.adreno_cmdbatch_sync())
        result.push_back("kgsl:adreno_cmdbatch_sync");
      if (kgsl.has_adreno_drawctxt_invalidate() && kgsl.adreno_drawctxt_invalidate())
        result.push_back("kgsl:adreno_drawctxt_invalidate");
      if (kgsl.has_adreno_drawctxt_sleep() && kgsl.adreno_drawctxt_sleep())
        result.push_back("kgsl:adreno_drawctxt_sleep");
      if (kgsl.has_adreno_drawctxt_switch() && kgsl.adreno_drawctxt_switch())
        result.push_back("kgsl:adreno_drawctxt_switch");
      if (kgsl.has_adreno_drawctxt_wait_done() && kgsl.adreno_drawctxt_wait_done())
        result.push_back("kgsl:adreno_drawctxt_wait_done");
      if (kgsl.has_adreno_drawctxt_wait_start() && kgsl.adreno_drawctxt_wait_start())
        result.push_back("kgsl:adreno_drawctxt_wait_start");
      if (kgsl.has_adreno_drawctxt_wake() && kgsl.adreno_drawctxt_wake())
        result.push_back("kgsl:adreno_drawctxt_wake");
      if (kgsl.has_adreno_gpu_fault() && kgsl.adreno_gpu_fault())
        result.push_back("kgsl:adreno_gpu_fault");
      if (kgsl.has_adreno_hw_preempt_clear_to_trig() && kgsl.adreno_hw_preempt_clear_to_trig())
        result.push_back("kgsl:adreno_hw_preempt_clear_to_trig");
      if (kgsl.has_adreno_hw_preempt_comp_to_clear() && kgsl.adreno_hw_preempt_comp_to_clear())
        result.push_back("kgsl:adreno_hw_preempt_comp_to_clear");
      if (kgsl.has_adreno_hw_preempt_token_submit() && kgsl.adreno_hw_preempt_token_submit())
        result.push_back("kgsl:adreno_hw_preempt_token_submit");
      if (kgsl.has_adreno_hw_preempt_trig_to_comp() && kgsl.adreno_hw_preempt_trig_to_comp())
        result.push_back("kgsl:adreno_hw_preempt_trig_to_comp");
      if (kgsl.has_adreno_hw_preempt_trig_to_comp_int() && kgsl.adreno_hw_preempt_trig_to_comp_int())
        result.push_back("kgsl:adreno_hw_preempt_trig_to_comp_int");
      if (kgsl.has_adreno_preempt_done() && kgsl.adreno_preempt_done())
        result.push_back("kgsl:adreno_preempt_done");
      if (kgsl.has_adreno_preempt_trigger() && kgsl.adreno_preempt_trigger())
        result.push_back("kgsl:adreno_preempt_trigger");
      if (kgsl.has_adreno_sp_tp() && kgsl.adreno_sp_tp())
        result.push_back("kgsl:adreno_sp_tp");
      if (kgsl.has_dispatch_queue_context() && kgsl.dispatch_queue_context())
        result.push_back("kgsl:dispatch_queue_context");
      if (kgsl.has_kgsl_a3xx_irq_status() && kgsl.kgsl_a3xx_irq_status())
        result.push_back("kgsl:kgsl_a3xx_irq_status");
      if (kgsl.has_kgsl_a4xx_irq_status() && kgsl.kgsl_a4xx_irq_status())
        result.push_back("kgsl:kgsl_a4xx_irq_status");
      if (kgsl.has_kgsl_a5xx_irq_status() && kgsl.kgsl_a5xx_irq_status())
        result.push_back("kgsl:kgsl_a5xx_irq_status");
      if (kgsl.has_kgsl_active_count() && kgsl.kgsl_active_count())
        result.push_back("kgsl:kgsl_active_count");
      if (kgsl.has_kgsl_bus() && kgsl.kgsl_bus())
        result.push_back("kgsl:kgsl_bus");
      if (kgsl.has_kgsl_buslevel() && kgsl.kgsl_buslevel())
        result.push_back("kgsl:kgsl_buslevel");
      if (kgsl.has_kgsl_clk() && kgsl.kgsl_clk())
        result.push_back("kgsl:kgsl_clk");
      if (kgsl.has_kgsl_constraint() && kgsl.kgsl_constraint())
        result.push_back("kgsl:kgsl_constraint");
      if (kgsl.has_kgsl_context_create() && kgsl.kgsl_context_create())
        result.push_back("kgsl:kgsl_context_create");
      if (kgsl.has_kgsl_context_destroy() && kgsl.kgsl_context_destroy())
        result.push_back("kgsl:kgsl_context_destroy");
      if (kgsl.has_kgsl_context_detach() && kgsl.kgsl_context_detach())
        result.push_back("kgsl:kgsl_context_detach");
      if (kgsl.has_kgsl_fire_event() && kgsl.kgsl_fire_event())
        result.push_back("kgsl:kgsl_fire_event");
      if (kgsl.has_kgsl_gpubusy() && kgsl.kgsl_gpubusy())
        result.push_back("kgsl:kgsl_gpubusy");
      if (kgsl.has_kgsl_irq() && kgsl.kgsl_irq())
        result.push_back("kgsl:kgsl_irq");
      if (kgsl.has_kgsl_issueibcmds() && kgsl.kgsl_issueibcmds())
        result.push_back("kgsl:kgsl_issueibcmds");
      if (kgsl.has_kgsl_mem_alloc() && kgsl.kgsl_mem_alloc())
        result.push_back("kgsl:kgsl_mem_alloc");
      if (kgsl.has_kgsl_mem_free() && kgsl.kgsl_mem_free())
        result.push_back("kgsl:kgsl_mem_free");
      if (kgsl.has_kgsl_mem_map() && kgsl.kgsl_mem_map())
        result.push_back("kgsl:kgsl_mem_map");
      if (kgsl.has_kgsl_mem_mmap() && kgsl.kgsl_mem_mmap())
        result.push_back("kgsl:kgsl_mem_mmap");
      if (kgsl.has_kgsl_mem_sync_cache() && kgsl.kgsl_mem_sync_cache())
        result.push_back("kgsl:kgsl_mem_sync_cache");
      if (kgsl.has_kgsl_mem_sync_full_cache() && kgsl.kgsl_mem_sync_full_cache())
        result.push_back("kgsl:kgsl_mem_sync_full_cache");
      if (kgsl.has_kgsl_mem_timestamp_free() && kgsl.kgsl_mem_timestamp_free())
        result.push_back("kgsl:kgsl_mem_timestamp_free");
      if (kgsl.has_kgsl_mem_timestamp_queue() && kgsl.kgsl_mem_timestamp_queue())
        result.push_back("kgsl:kgsl_mem_timestamp_queue");
      if (kgsl.has_kgsl_mem_unmapped_area_collision() && kgsl.kgsl_mem_unmapped_area_collision())
        result.push_back("kgsl:kgsl_mem_unmapped_area_collision");
      if (kgsl.has_kgsl_mmu_pagefault() && kgsl.kgsl_mmu_pagefault())
        result.push_back("kgsl:kgsl_mmu_pagefault");
      if (kgsl.has_kgsl_msg() && kgsl.kgsl_msg())
        result.push_back("kgsl:kgsl_msg");
      if (kgsl.has_kgsl_pagetable_destroy() && kgsl.kgsl_pagetable_destroy())
        result.push_back("kgsl:kgsl_pagetable_destroy");
      if (kgsl.has_kgsl_popp_level() && kgsl.kgsl_popp_level())
        result.push_back("kgsl:kgsl_popp_level");
      if (kgsl.has_kgsl_popp_mod() && kgsl.kgsl_popp_mod())
        result.push_back("kgsl:kgsl_popp_mod");
      if (kgsl.has_kgsl_popp_nap() && kgsl.kgsl_popp_nap())
        result.push_back("kgsl:kgsl_popp_nap");
      if (kgsl.has_kgsl_pwr_request_state() && kgsl.kgsl_pwr_request_state())
        result.push_back("kgsl:kgsl_pwr_request_state");
      if (kgsl.has_kgsl_pwr_set_state() && kgsl.kgsl_pwr_set_state())
        result.push_back("kgsl:kgsl_pwr_set_state");
      if (kgsl.has_kgsl_pwrlevel() && kgsl.kgsl_pwrlevel())
        result.push_back("kgsl:kgsl_pwrlevel");
      if (kgsl.has_kgsl_pwrstats() && kgsl.kgsl_pwrstats())
        result.push_back("kgsl:kgsl_pwrstats");
      if (kgsl.has_kgsl_rail() && kgsl.kgsl_rail())
        result.push_back("kgsl:kgsl_rail");
      if (kgsl.has_kgsl_readtimestamp() && kgsl.kgsl_readtimestamp())
        result.push_back("kgsl:kgsl_readtimestamp");
      if (kgsl.has_kgsl_register_event() && kgsl.kgsl_register_event())
        result.push_back("kgsl:kgsl_register_event");
      if (kgsl.has_kgsl_regwrite() && kgsl.kgsl_regwrite())
        result.push_back("kgsl:kgsl_regwrite");
      if (kgsl.has_kgsl_retention_clk() && kgsl.kgsl_retention_clk())
        result.push_back("kgsl:kgsl_retention_clk");
      if (kgsl.has_kgsl_user_pwrlevel_constraint() && kgsl.kgsl_user_pwrlevel_constraint())
        result.push_back("kgsl:kgsl_user_pwrlevel_constraint");
      if (kgsl.has_kgsl_waittimestamp_entry() && kgsl.kgsl_waittimestamp_entry())
        result.push_back("kgsl:kgsl_waittimestamp_entry");
      if (kgsl.has_kgsl_waittimestamp_exit() && kgsl.kgsl_waittimestamp_exit())
        result.push_back("kgsl:kgsl_waittimestamp_exit");
      if (kgsl.has_syncpoint_fence() && kgsl.syncpoint_fence())
        result.push_back("kgsl:syncpoint_fence");
      if (kgsl.has_syncpoint_fence_expire() && kgsl.syncpoint_fence_expire())
        result.push_back("kgsl:syncpoint_fence_expire");
      if (kgsl.has_syncpoint_timestamp() && kgsl.syncpoint_timestamp())
        result.push_back("kgsl:syncpoint_timestamp");
      if (kgsl.has_syncpoint_timestamp_expire() && kgsl.syncpoint_timestamp_expire())
        result.push_back("kgsl:syncpoint_timestamp_expire");
    }

    if (tracepoints.has_kmem()) {
      const auto& kmem = tracepoints.kmem();

      if (kmem.has_alloc_pages_iommu_end() && kmem.alloc_pages_iommu_end())
        result.push_back("kmem:alloc_pages_iommu_end");
      if (kmem.has_alloc_pages_iommu_fail() && kmem.alloc_pages_iommu_fail())
        result.push_back("kmem:alloc_pages_iommu_fail");
      if (kmem.has_alloc_pages_iommu_start() && kmem.alloc_pages_iommu_start())
        result.push_back("kmem:alloc_pages_iommu_start");
      if (kmem.has_alloc_pages_sys_end() && kmem.alloc_pages_sys_end())
        result.push_back("kmem:alloc_pages_sys_end");
      if (kmem.has_alloc_pages_sys_fail() && kmem.alloc_pages_sys_fail())
        result.push_back("kmem:alloc_pages_sys_fail");
      if (kmem.has_alloc_pages_sys_start() && kmem.alloc_pages_sys_start())
        result.push_back("kmem:alloc_pages_sys_start");
      if (kmem.has_dma_alloc_contiguous_retry() && kmem.dma_alloc_contiguous_retry())
        result.push_back("kmem:dma_alloc_contiguous_retry");
      if (kmem.has_iommu_map_range() && kmem.iommu_map_range())
        result.push_back("kmem:iommu_map_range");
      if (kmem.has_iommu_sec_ptbl_map_range_end() && kmem.iommu_sec_ptbl_map_range_end())
        result.push_back("kmem:iommu_sec_ptbl_map_range_end");
      if (kmem.has_iommu_sec_ptbl_map_range_start() && kmem.iommu_sec_ptbl_map_range_start())
        result.push_back("kmem:iommu_sec_ptbl_map_range_start");
      if (kmem.has_ion_alloc_buffer_end() && kmem.ion_alloc_buffer_end())
        result.push_back("kmem:ion_alloc_buffer_end");
      if (kmem.has_ion_alloc_buffer_fail() && kmem.ion_alloc_buffer_fail())
        result.push_back("kmem:ion_alloc_buffer_fail");
      if (kmem.has_ion_alloc_buffer_fallback() && kmem.ion_alloc_buffer_fallback())
        result.push_back("kmem:ion_alloc_buffer_fallback");
      if (kmem.has_ion_alloc_buffer_start() && kmem.ion_alloc_buffer_start())
        result.push_back("kmem:ion_alloc_buffer_start");
      if (kmem.has_ion_cp_alloc_retry() && kmem.ion_cp_alloc_retry())
        result.push_back("kmem:ion_cp_alloc_retry");
      if (kmem.has_ion_cp_secure_buffer_end() && kmem.ion_cp_secure_buffer_end())
        result.push_back("kmem:ion_cp_secure_buffer_end");
      if (kmem.has_ion_cp_secure_buffer_start() && kmem.ion_cp_secure_buffer_start())
        result.push_back("kmem:ion_cp_secure_buffer_start");
      if (kmem.has_ion_prefetching() && kmem.ion_prefetching())
        result.push_back("kmem:ion_prefetching");
      if (kmem.has_ion_secure_cma_add_to_pool_end() && kmem.ion_secure_cma_add_to_pool_end())
        result.push_back("kmem:ion_secure_cma_add_to_pool_end");
      if (kmem.has_ion_secure_cma_add_to_pool_start() && kmem.ion_secure_cma_add_to_pool_start())
        result.push_back("kmem:ion_secure_cma_add_to_pool_start");
      if (kmem.has_ion_secure_cma_allocate_end() && kmem.ion_secure_cma_allocate_end())
        result.push_back("kmem:ion_secure_cma_allocate_end");
      if (kmem.has_ion_secure_cma_allocate_start() && kmem.ion_secure_cma_allocate_start())
        result.push_back("kmem:ion_secure_cma_allocate_start");
      if (kmem.has_ion_secure_cma_shrink_pool_end() && kmem.ion_secure_cma_shrink_pool_end())
        result.push_back("kmem:ion_secure_cma_shrink_pool_end");
      if (kmem.has_ion_secure_cma_shrink_pool_start() && kmem.ion_secure_cma_shrink_pool_start())
        result.push_back("kmem:ion_secure_cma_shrink_pool_start");
      if (kmem.has_kfree() && kmem.kfree())
        result.push_back("kmem:kfree");
      if (kmem.has_kmalloc() && kmem.kmalloc())
        result.push_back("kmem:kmalloc");
      if (kmem.has_kmalloc_node() && kmem.kmalloc_node())
        result.push_back("kmem:kmalloc_node");
      if (kmem.has_kmem_cache_alloc() && kmem.kmem_cache_alloc())
        result.push_back("kmem:kmem_cache_alloc");
      if (kmem.has_kmem_cache_alloc_node() && kmem.kmem_cache_alloc_node())
        result.push_back("kmem:kmem_cache_alloc_node");
      if (kmem.has_kmem_cache_free() && kmem.kmem_cache_free())
        result.push_back("kmem:kmem_cache_free");
      if (kmem.has_migrate_pages_end() && kmem.migrate_pages_end())
        result.push_back("kmem:migrate_pages_end");
      if (kmem.has_migrate_pages_start() && kmem.migrate_pages_start())
        result.push_back("kmem:migrate_pages_start");
      if (kmem.has_migrate_retry() && kmem.migrate_retry())
        result.push_back("kmem:migrate_retry");
      if (kmem.has_mm_page_alloc() && kmem.mm_page_alloc())
        result.push_back("kmem:mm_page_alloc");
      if (kmem.has_mm_page_alloc_extfrag() && kmem.mm_page_alloc_extfrag())
        result.push_back("kmem:mm_page_alloc_extfrag");
      if (kmem.has_mm_page_alloc_zone_locked() && kmem.mm_page_alloc_zone_locked())
        result.push_back("kmem:mm_page_alloc_zone_locked");
      if (kmem.has_mm_page_free() && kmem.mm_page_free())
        result.push_back("kmem:mm_page_free");
      if (kmem.has_mm_page_free_batched() && kmem.mm_page_free_batched())
        result.push_back("kmem:mm_page_free_batched");
      if (kmem.has_mm_page_pcpu_drain() && kmem.mm_page_pcpu_drain())
        result.push_back("kmem:mm_page_pcpu_drain");
    }

    if (tracepoints.has_lowmemorykiller()) {
      const auto& lowmemorykiller = tracepoints.lowmemorykiller();

      if (lowmemorykiller.has_lowmemory_kill() && lowmemorykiller.lowmemory_kill())
        result.push_back("lowmemorykiller:lowmemory_kill");
    }

    if (tracepoints.has_mdss()) {
      const auto& mdss = tracepoints.mdss();

      if (mdss.has_mdp_cmd_kickoff() && mdss.mdp_cmd_kickoff())
        result.push_back("mdss:mdp_cmd_kickoff");
      if (mdss.has_mdp_cmd_pingpong_done() && mdss.mdp_cmd_pingpong_done())
        result.push_back("mdss:mdp_cmd_pingpong_done");
      if (mdss.has_mdp_cmd_release_bw() && mdss.mdp_cmd_release_bw())
        result.push_back("mdss:mdp_cmd_release_bw");
      if (mdss.has_mdp_cmd_wait_pingpong() && mdss.mdp_cmd_wait_pingpong())
        result.push_back("mdss:mdp_cmd_wait_pingpong");
      if (mdss.has_mdp_commit() && mdss.mdp_commit())
        result.push_back("mdss:mdp_commit");
      if (mdss.has_mdp_misr_crc() && mdss.mdp_misr_crc())
        result.push_back("mdss:mdp_misr_crc");
      if (mdss.has_mdp_mixer_update() && mdss.mdp_mixer_update())
        result.push_back("mdss:mdp_mixer_update");
      if (mdss.has_mdp_perf_prefill_calc() && mdss.mdp_perf_prefill_calc())
        result.push_back("mdss:mdp_perf_prefill_calc");
      if (mdss.has_mdp_perf_set_ot() && mdss.mdp_perf_set_ot())
        result.push_back("mdss:mdp_perf_set_ot");
      if (mdss.has_mdp_perf_set_panic_luts() && mdss.mdp_perf_set_panic_luts())
        result.push_back("mdss:mdp_perf_set_panic_luts");
      if (mdss.has_mdp_perf_set_qos_luts() && mdss.mdp_perf_set_qos_luts())
        result.push_back("mdss:mdp_perf_set_qos_luts");
      if (mdss.has_mdp_perf_set_wm_levels() && mdss.mdp_perf_set_wm_levels())
        result.push_back("mdss:mdp_perf_set_wm_levels");
      if (mdss.has_mdp_perf_update_bus() && mdss.mdp_perf_update_bus())
        result.push_back("mdss:mdp_perf_update_bus");
      if (mdss.has_mdp_sspp_change() && mdss.mdp_sspp_change())
        result.push_back("mdss:mdp_sspp_change");
      if (mdss.has_mdp_sspp_set() && mdss.mdp_sspp_set())
        result.push_back("mdss:mdp_sspp_set");
      if (mdss.has_mdp_trace_counter() && mdss.mdp_trace_counter())
        result.push_back("mdss:mdp_trace_counter");
      if (mdss.has_mdp_video_underrun_done() && mdss.mdp_video_underrun_done())
        result.push_back("mdss:mdp_video_underrun_done");
      if (mdss.has_rotator_bw_ao_as_context() && mdss.rotator_bw_ao_as_context())
        result.push_back("mdss:rotator_bw_ao_as_context");
      if (mdss.has_tracing_mark_write() && mdss.tracing_mark_write())
        result.push_back("mdss:tracing_mark_write");
    }

    if (tracepoints.has_migrate()) {
      const auto& migrate = tracepoints.migrate();

      if (migrate.has_mm_migrate_pages() && migrate.mm_migrate_pages())
        result.push_back("migrate:mm_migrate_pages");
      if (migrate.has_mm_migrate_pages_start() && migrate.mm_migrate_pages_start())
        result.push_back("migrate:mm_migrate_pages_start");
      if (migrate.has_mm_numa_migrate_ratelimit() && migrate.mm_numa_migrate_ratelimit())
        result.push_back("migrate:mm_numa_migrate_ratelimit");
    }

    if (tracepoints.has_module()) {
      const auto& module = tracepoints.module();

      if (module.has_module_free() && module.module_free())
        result.push_back("module:module_free");
      if (module.has_module_get() && module.module_get())
        result.push_back("module:module_get");
      if (module.has_module_load() && module.module_load())
        result.push_back("module:module_load");
      if (module.has_module_put() && module.module_put())
        result.push_back("module:module_put");
      if (module.has_module_request() && module.module_request())
        result.push_back("module:module_request");
    }

    if (tracepoints.has_msm_bus()) {
      const auto& msm_bus = tracepoints.msm_bus();

      if (msm_bus.has_bus_agg_bw() && msm_bus.bus_agg_bw())
        result.push_back("msm_bus:bus_agg_bw");
      if (msm_bus.has_bus_avail_bw() && msm_bus.bus_avail_bw())
        result.push_back("msm_bus:bus_avail_bw");
      if (msm_bus.has_bus_bimc_config_limiter() && msm_bus.bus_bimc_config_limiter())
        result.push_back("msm_bus:bus_bimc_config_limiter");
      if (msm_bus.has_bus_bke_params() && msm_bus.bus_bke_params())
        result.push_back("msm_bus:bus_bke_params");
      if (msm_bus.has_bus_client_status() && msm_bus.bus_client_status())
        result.push_back("msm_bus:bus_client_status");
      if (msm_bus.has_bus_rules_matches() && msm_bus.bus_rules_matches())
        result.push_back("msm_bus:bus_rules_matches");
      if (msm_bus.has_bus_update_request() && msm_bus.bus_update_request())
        result.push_back("msm_bus:bus_update_request");
      if (msm_bus.has_bus_update_request_end() && msm_bus.bus_update_request_end())
        result.push_back("msm_bus:bus_update_request_end");
    }

    if (tracepoints.has_msm_low_power()) {
      const auto& msm_low_power = tracepoints.msm_low_power();

      if (msm_low_power.has_cluster_enter() && msm_low_power.cluster_enter())
        result.push_back("msm_low_power:cluster_enter");
      if (msm_low_power.has_cluster_exit() && msm_low_power.cluster_exit())
        result.push_back("msm_low_power:cluster_exit");
      if (msm_low_power.has_cpu_idle_enter() && msm_low_power.cpu_idle_enter())
        result.push_back("msm_low_power:cpu_idle_enter");
      if (msm_low_power.has_cpu_idle_exit() && msm_low_power.cpu_idle_exit())
        result.push_back("msm_low_power:cpu_idle_exit");
      if (msm_low_power.has_cpu_power_select() && msm_low_power.cpu_power_select())
        result.push_back("msm_low_power:cpu_power_select");
      if (msm_low_power.has_pre_pc_cb() && msm_low_power.pre_pc_cb())
        result.push_back("msm_low_power:pre_pc_cb");
    }

    if (tracepoints.has_msm_vidc()) {
      const auto& msm_vidc = tracepoints.msm_vidc();

      if (msm_vidc.has_msm_smem_buffer_iommu_op_end() && msm_vidc.msm_smem_buffer_iommu_op_end())
        result.push_back("msm_vidc:msm_smem_buffer_iommu_op_end");
      if (msm_vidc.has_msm_smem_buffer_iommu_op_start() && msm_vidc.msm_smem_buffer_iommu_op_start())
        result.push_back("msm_vidc:msm_smem_buffer_iommu_op_start");
      if (msm_vidc.has_msm_smem_buffer_ion_op_end() && msm_vidc.msm_smem_buffer_ion_op_end())
        result.push_back("msm_vidc:msm_smem_buffer_ion_op_end");
      if (msm_vidc.has_msm_smem_buffer_ion_op_start() && msm_vidc.msm_smem_buffer_ion_op_start())
        result.push_back("msm_vidc:msm_smem_buffer_ion_op_start");
      if (msm_vidc.has_msm_v4l2_vidc_buffer_event_end() && msm_vidc.msm_v4l2_vidc_buffer_event_end())
        result.push_back("msm_vidc:msm_v4l2_vidc_buffer_event_end");
      if (msm_vidc.has_msm_v4l2_vidc_buffer_event_start() && msm_vidc.msm_v4l2_vidc_buffer_event_start())
        result.push_back("msm_vidc:msm_v4l2_vidc_buffer_event_start");
      if (msm_vidc.has_msm_v4l2_vidc_close_end() && msm_vidc.msm_v4l2_vidc_close_end())
        result.push_back("msm_vidc:msm_v4l2_vidc_close_end");
      if (msm_vidc.has_msm_v4l2_vidc_close_start() && msm_vidc.msm_v4l2_vidc_close_start())
        result.push_back("msm_vidc:msm_v4l2_vidc_close_start");
      if (msm_vidc.has_msm_v4l2_vidc_fw_load_end() && msm_vidc.msm_v4l2_vidc_fw_load_end())
        result.push_back("msm_vidc:msm_v4l2_vidc_fw_load_end");
      if (msm_vidc.has_msm_v4l2_vidc_fw_load_start() && msm_vidc.msm_v4l2_vidc_fw_load_start())
        result.push_back("msm_vidc:msm_v4l2_vidc_fw_load_start");
      if (msm_vidc.has_msm_v4l2_vidc_open_end() && msm_vidc.msm_v4l2_vidc_open_end())
        result.push_back("msm_vidc:msm_v4l2_vidc_open_end");
      if (msm_vidc.has_msm_v4l2_vidc_open_start() && msm_vidc.msm_v4l2_vidc_open_start())
        result.push_back("msm_vidc:msm_v4l2_vidc_open_start");
      if (msm_vidc.has_msm_vidc_common_state_change() && msm_vidc.msm_vidc_common_state_change())
        result.push_back("msm_vidc:msm_vidc_common_state_change");
      if (msm_vidc.has_venus_hfi_var_done() && msm_vidc.venus_hfi_var_done())
        result.push_back("msm_vidc:venus_hfi_var_done");
    }

    if (tracepoints.has_napi()) {
      const auto& napi = tracepoints.napi();

      if (napi.has_napi_poll() && napi.napi_poll())
        result.push_back("napi:napi_poll");
    }

    if (tracepoints.has_net()) {
      const auto& net = tracepoints.net();

      if (net.has_napi_gro_frags_entry() && net.napi_gro_frags_entry())
        result.push_back("net:napi_gro_frags_entry");
      if (net.has_napi_gro_receive_entry() && net.napi_gro_receive_entry())
        result.push_back("net:napi_gro_receive_entry");
      if (net.has_net_dev_queue() && net.net_dev_queue())
        result.push_back("net:net_dev_queue");
      if (net.has_net_dev_start_xmit() && net.net_dev_start_xmit())
        result.push_back("net:net_dev_start_xmit");
      if (net.has_net_dev_xmit() && net.net_dev_xmit())
        result.push_back("net:net_dev_xmit");
      if (net.has_netif_receive_skb() && net.netif_receive_skb())
        result.push_back("net:netif_receive_skb");
      if (net.has_netif_receive_skb_entry() && net.netif_receive_skb_entry())
        result.push_back("net:netif_receive_skb_entry");
      if (net.has_netif_rx() && net.netif_rx())
        result.push_back("net:netif_rx");
      if (net.has_netif_rx_entry() && net.netif_rx_entry())
        result.push_back("net:netif_rx_entry");
      if (net.has_netif_rx_ni_entry() && net.netif_rx_ni_entry())
        result.push_back("net:netif_rx_ni_entry");
    }

    if (tracepoints.has_oom()) {
      const auto& oom = tracepoints.oom();

      if (oom.has_oom_score_adj_update() && oom.oom_score_adj_update())
        result.push_back("oom:oom_score_adj_update");
    }

    if (tracepoints.has_pagemap()) {
      const auto& pagemap = tracepoints.pagemap();

      if (pagemap.has_mm_lru_activate() && pagemap.mm_lru_activate())
        result.push_back("pagemap:mm_lru_activate");
      if (pagemap.has_mm_lru_insertion() && pagemap.mm_lru_insertion())
        result.push_back("pagemap:mm_lru_insertion");
    }

    if (tracepoints.has_perf_trace_counters()) {
      const auto& perf_trace_counters = tracepoints.perf_trace_counters();

      if (perf_trace_counters.has_perf_trace_user() && perf_trace_counters.perf_trace_user())
        result.push_back("perf_trace_counters:perf_trace_user");
      if (perf_trace_counters.has_sched_switch_with_ctrs() && perf_trace_counters.sched_switch_with_ctrs())
        result.push_back("perf_trace_counters:sched_switch_with_ctrs");
    }

    if (tracepoints.has_power()) {
      const auto& power = tracepoints.power();

      if (power.has_bw_hwmon_meas() && power.bw_hwmon_meas())
        result.push_back("power:bw_hwmon_meas");
      if (power.has_bw_hwmon_update() && power.bw_hwmon_update())
        result.push_back("power:bw_hwmon_update");
      if (power.has_cache_hwmon_meas() && power.cache_hwmon_meas())
        result.push_back("power:cache_hwmon_meas");
      if (power.has_cache_hwmon_update() && power.cache_hwmon_update())
        result.push_back("power:cache_hwmon_update");
      if (power.has_clock_disable() && power.clock_disable())
        result.push_back("power:clock_disable");
      if (power.has_clock_enable() && power.clock_enable())
        result.push_back("power:clock_enable");
      if (power.has_clock_set_parent() && power.clock_set_parent())
        result.push_back("power:clock_set_parent");
      if (power.has_clock_set_rate() && power.clock_set_rate())
        result.push_back("power:clock_set_rate");
      if (power.has_clock_set_rate_complete() && power.clock_set_rate_complete())
        result.push_back("power:clock_set_rate_complete");
      if (power.has_clock_state() && power.clock_state())
        result.push_back("power:clock_state");
      if (power.has_core_ctl_eval_need() && power.core_ctl_eval_need())
        result.push_back("power:core_ctl_eval_need");
      if (power.has_core_ctl_set_busy() && power.core_ctl_set_busy())
        result.push_back("power:core_ctl_set_busy");
      if (power.has_cpu_capacity() && power.cpu_capacity())
        result.push_back("power:cpu_capacity");
      if (power.has_cpu_frequency() && power.cpu_frequency())
        result.push_back("power:cpu_frequency");
      if (power.has_cpu_frequency_limits() && power.cpu_frequency_limits())
        result.push_back("power:cpu_frequency_limits");
      if (power.has_cpu_frequency_switch_end() && power.cpu_frequency_switch_end())
        result.push_back("power:cpu_frequency_switch_end");
      if (power.has_cpu_frequency_switch_start() && power.cpu_frequency_switch_start())
        result.push_back("power:cpu_frequency_switch_start");
      if (power.has_cpu_idle() && power.cpu_idle())
        result.push_back("power:cpu_idle");
      if (power.has_cpu_mode_detect() && power.cpu_mode_detect())
        result.push_back("power:cpu_mode_detect");
      if (power.has_cpufreq_freq_synced() && power.cpufreq_freq_synced())
        result.push_back("power:cpufreq_freq_synced");
      if (power.has_cpufreq_sampling_event() && power.cpufreq_sampling_event())
        result.push_back("power:cpufreq_sampling_event");
      if (power.has_dev_pm_qos_add_request() && power.dev_pm_qos_add_request())
        result.push_back("power:dev_pm_qos_add_request");
      if (power.has_dev_pm_qos_remove_request() && power.dev_pm_qos_remove_request())
        result.push_back("power:dev_pm_qos_remove_request");
      if (power.has_dev_pm_qos_update_request() && power.dev_pm_qos_update_request())
        result.push_back("power:dev_pm_qos_update_request");
      if (power.has_device_pm_callback_end() && power.device_pm_callback_end())
        result.push_back("power:device_pm_callback_end");
      if (power.has_device_pm_callback_start() && power.device_pm_callback_start())
        result.push_back("power:device_pm_callback_start");
      if (power.has_memlat_dev_meas() && power.memlat_dev_meas())
        result.push_back("power:memlat_dev_meas");
      if (power.has_memlat_dev_update() && power.memlat_dev_update())
        result.push_back("power:memlat_dev_update");
      if (power.has_msmpower_max_ddr() && power.msmpower_max_ddr())
        result.push_back("power:msmpower_max_ddr");
      if (power.has_perf_cl_peak_exit_timer_start() && power.perf_cl_peak_exit_timer_start())
        result.push_back("power:perf_cl_peak_exit_timer_start");
      if (power.has_perf_cl_peak_exit_timer_stop() && power.perf_cl_peak_exit_timer_stop())
        result.push_back("power:perf_cl_peak_exit_timer_stop");
      if (power.has_pm_qos_add_request() && power.pm_qos_add_request())
        result.push_back("power:pm_qos_add_request");
      if (power.has_pm_qos_remove_request() && power.pm_qos_remove_request())
        result.push_back("power:pm_qos_remove_request");
      if (power.has_pm_qos_update_flags() && power.pm_qos_update_flags())
        result.push_back("power:pm_qos_update_flags");
      if (power.has_pm_qos_update_request() && power.pm_qos_update_request())
        result.push_back("power:pm_qos_update_request");
      if (power.has_pm_qos_update_request_timeout() && power.pm_qos_update_request_timeout())
        result.push_back("power:pm_qos_update_request_timeout");
      if (power.has_pm_qos_update_target() && power.pm_qos_update_target())
        result.push_back("power:pm_qos_update_target");
      if (power.has_power_domain_target() && power.power_domain_target())
        result.push_back("power:power_domain_target");
      if (power.has_pstate_sample() && power.pstate_sample())
        result.push_back("power:pstate_sample");
      if (power.has_reevaluate_hotplug() && power.reevaluate_hotplug())
        result.push_back("power:reevaluate_hotplug");
      if (power.has_set_max_cpus() && power.set_max_cpus())
        result.push_back("power:set_max_cpus");
      if (power.has_single_cycle_exit_timer_start() && power.single_cycle_exit_timer_start())
        result.push_back("power:single_cycle_exit_timer_start");
      if (power.has_single_cycle_exit_timer_stop() && power.single_cycle_exit_timer_stop())
        result.push_back("power:single_cycle_exit_timer_stop");
      if (power.has_single_mode_timeout() && power.single_mode_timeout())
        result.push_back("power:single_mode_timeout");
      if (power.has_suspend_resume() && power.suspend_resume())
        result.push_back("power:suspend_resume");
      if (power.has_track_iowait() && power.track_iowait())
        result.push_back("power:track_iowait");
      if (power.has_wakeup_source_activate() && power.wakeup_source_activate())
        result.push_back("power:wakeup_source_activate");
      if (power.has_wakeup_source_deactivate() && power.wakeup_source_deactivate())
        result.push_back("power:wakeup_source_deactivate");
    }

    if (tracepoints.has_printk()) {
      const auto& printk = tracepoints.printk();

      if (printk.has_console() && printk.console())
        result.push_back("printk:console");
    }

    if (tracepoints.has_random()) {
      const auto& random = tracepoints.random();

      if (random.has_add_device_randomness() && random.add_device_randomness())
        result.push_back("random:add_device_randomness");
      if (random.has_add_disk_randomness() && random.add_disk_randomness())
        result.push_back("random:add_disk_randomness");
      if (random.has_add_input_randomness() && random.add_input_randomness())
        result.push_back("random:add_input_randomness");
      if (random.has_credit_entropy_bits() && random.credit_entropy_bits())
        result.push_back("random:credit_entropy_bits");
      if (random.has_debit_entropy() && random.debit_entropy())
        result.push_back("random:debit_entropy");
      if (random.has_extract_entropy() && random.extract_entropy())
        result.push_back("random:extract_entropy");
      if (random.has_extract_entropy_user() && random.extract_entropy_user())
        result.push_back("random:extract_entropy_user");
      if (random.has_get_random_bytes() && random.get_random_bytes())
        result.push_back("random:get_random_bytes");
      if (random.has_get_random_bytes_arch() && random.get_random_bytes_arch())
        result.push_back("random:get_random_bytes_arch");
      if (random.has_mix_pool_bytes() && random.mix_pool_bytes())
        result.push_back("random:mix_pool_bytes");
      if (random.has_mix_pool_bytes_nolock() && random.mix_pool_bytes_nolock())
        result.push_back("random:mix_pool_bytes_nolock");
      if (random.has_push_to_pool() && random.push_to_pool())
        result.push_back("random:push_to_pool");
      if (random.has_random_read() && random.random_read())
        result.push_back("random:random_read");
      if (random.has_urandom_read() && random.urandom_read())
        result.push_back("random:urandom_read");
      if (random.has_xfer_secondary_pool() && random.xfer_secondary_pool())
        result.push_back("random:xfer_secondary_pool");
    }

    if (tracepoints.has_raw_syscalls()) {
      const auto& raw_syscalls = tracepoints.raw_syscalls();

      if (raw_syscalls.has_sys_enter() && raw_syscalls.sys_enter())
        result.push_back("raw_syscalls:sys_enter");
      if (raw_syscalls.has_sys_exit() && raw_syscalls.sys_exit())
        result.push_back("raw_syscalls:sys_exit");
    }

    if (tracepoints.has_rcu()) {
      const auto& rcu = tracepoints.rcu();

      if (rcu.has_rcu_utilization() && rcu.rcu_utilization())
        result.push_back("rcu:rcu_utilization");
    }

    if (tracepoints.has_regmap()) {
      const auto& regmap = tracepoints.regmap();

      if (regmap.has_regcache_drop_region() && regmap.regcache_drop_region())
        result.push_back("regmap:regcache_drop_region");
      if (regmap.has_regcache_sync() && regmap.regcache_sync())
        result.push_back("regmap:regcache_sync");
      if (regmap.has_regmap_async_complete_done() && regmap.regmap_async_complete_done())
        result.push_back("regmap:regmap_async_complete_done");
      if (regmap.has_regmap_async_complete_start() && regmap.regmap_async_complete_start())
        result.push_back("regmap:regmap_async_complete_start");
      if (regmap.has_regmap_async_io_complete() && regmap.regmap_async_io_complete())
        result.push_back("regmap:regmap_async_io_complete");
      if (regmap.has_regmap_async_write_start() && regmap.regmap_async_write_start())
        result.push_back("regmap:regmap_async_write_start");
      if (regmap.has_regmap_cache_bypass() && regmap.regmap_cache_bypass())
        result.push_back("regmap:regmap_cache_bypass");
      if (regmap.has_regmap_cache_only() && regmap.regmap_cache_only())
        result.push_back("regmap:regmap_cache_only");
      if (regmap.has_regmap_hw_read_done() && regmap.regmap_hw_read_done())
        result.push_back("regmap:regmap_hw_read_done");
      if (regmap.has_regmap_hw_read_start() && regmap.regmap_hw_read_start())
        result.push_back("regmap:regmap_hw_read_start");
      if (regmap.has_regmap_hw_write_done() && regmap.regmap_hw_write_done())
        result.push_back("regmap:regmap_hw_write_done");
      if (regmap.has_regmap_hw_write_start() && regmap.regmap_hw_write_start())
        result.push_back("regmap:regmap_hw_write_start");
      if (regmap.has_regmap_reg_read() && regmap.regmap_reg_read())
        result.push_back("regmap:regmap_reg_read");
      if (regmap.has_regmap_reg_read_cache() && regmap.regmap_reg_read_cache())
        result.push_back("regmap:regmap_reg_read_cache");
      if (regmap.has_regmap_reg_write() && regmap.regmap_reg_write())
        result.push_back("regmap:regmap_reg_write");
    }

    if (tracepoints.has_regulator()) {
      const auto& regulator = tracepoints.regulator();

      if (regulator.has_regulator_disable() && regulator.regulator_disable())
        result.push_back("regulator:regulator_disable");
      if (regulator.has_regulator_disable_complete() && regulator.regulator_disable_complete())
        result.push_back("regulator:regulator_disable_complete");
      if (regulator.has_regulator_enable() && regulator.regulator_enable())
        result.push_back("regulator:regulator_enable");
      if (regulator.has_regulator_enable_complete() && regulator.regulator_enable_complete())
        result.push_back("regulator:regulator_enable_complete");
      if (regulator.has_regulator_enable_delay() && regulator.regulator_enable_delay())
        result.push_back("regulator:regulator_enable_delay");
      if (regulator.has_regulator_set_voltage() && regulator.regulator_set_voltage())
        result.push_back("regulator:regulator_set_voltage");
      if (regulator.has_regulator_set_voltage_complete() && regulator.regulator_set_voltage_complete())
        result.push_back("regulator:regulator_set_voltage_complete");
    }

    if (tracepoints.has_rmnet_data()) {
      const auto& rmnet_data = tracepoints.rmnet_data();

      if (rmnet_data.has___rmnet_deliver_skb() && rmnet_data.__rmnet_deliver_skb())
        result.push_back("rmnet_data:__rmnet_deliver_skb");
      if (rmnet_data.has_rmnet_associate() && rmnet_data.rmnet_associate())
        result.push_back("rmnet_data:rmnet_associate");
      if (rmnet_data.has_rmnet_egress_handler() && rmnet_data.rmnet_egress_handler())
        result.push_back("rmnet_data:rmnet_egress_handler");
      if (rmnet_data.has_rmnet_end_deaggregation() && rmnet_data.rmnet_end_deaggregation())
        result.push_back("rmnet_data:rmnet_end_deaggregation");
      if (rmnet_data.has_rmnet_fc_map() && rmnet_data.rmnet_fc_map())
        result.push_back("rmnet_data:rmnet_fc_map");
      if (rmnet_data.has_rmnet_fc_qmi() && rmnet_data.rmnet_fc_qmi())
        result.push_back("rmnet_data:rmnet_fc_qmi");
      if (rmnet_data.has_rmnet_gro_downlink() && rmnet_data.rmnet_gro_downlink())
        result.push_back("rmnet_data:rmnet_gro_downlink");
      if (rmnet_data.has_rmnet_ingress_handler() && rmnet_data.rmnet_ingress_handler())
        result.push_back("rmnet_data:rmnet_ingress_handler");
      if (rmnet_data.has_rmnet_map_aggregate() && rmnet_data.rmnet_map_aggregate())
        result.push_back("rmnet_data:rmnet_map_aggregate");
      if (rmnet_data.has_rmnet_map_checksum_downlink_packet() && rmnet_data.rmnet_map_checksum_downlink_packet())
        result.push_back("rmnet_data:rmnet_map_checksum_downlink_packet");
      if (rmnet_data.has_rmnet_map_checksum_uplink_packet() && rmnet_data.rmnet_map_checksum_uplink_packet())
        result.push_back("rmnet_data:rmnet_map_checksum_uplink_packet");
      if (rmnet_data.has_rmnet_map_flush_packet_queue() && rmnet_data.rmnet_map_flush_packet_queue())
        result.push_back("rmnet_data:rmnet_map_flush_packet_queue");
      if (rmnet_data.has_rmnet_start_aggregation() && rmnet_data.rmnet_start_aggregation())
        result.push_back("rmnet_data:rmnet_start_aggregation");
      if (rmnet_data.has_rmnet_start_deaggregation() && rmnet_data.rmnet_start_deaggregation())
        result.push_back("rmnet_data:rmnet_start_deaggregation");
      if (rmnet_data.has_rmnet_unassociate() && rmnet_data.rmnet_unassociate())
        result.push_back("rmnet_data:rmnet_unassociate");
      if (rmnet_data.has_rmnet_unregister_cb_clear_lepcs() && rmnet_data.rmnet_unregister_cb_clear_lepcs())
        result.push_back("rmnet_data:rmnet_unregister_cb_clear_lepcs");
      if (rmnet_data.has_rmnet_unregister_cb_clear_vnds() && rmnet_data.rmnet_unregister_cb_clear_vnds())
        result.push_back("rmnet_data:rmnet_unregister_cb_clear_vnds");
      if (rmnet_data.has_rmnet_unregister_cb_entry() && rmnet_data.rmnet_unregister_cb_entry())
        result.push_back("rmnet_data:rmnet_unregister_cb_entry");
      if (rmnet_data.has_rmnet_unregister_cb_exit() && rmnet_data.rmnet_unregister_cb_exit())
        result.push_back("rmnet_data:rmnet_unregister_cb_exit");
      if (rmnet_data.has_rmnet_unregister_cb_unhandled() && rmnet_data.rmnet_unregister_cb_unhandled())
        result.push_back("rmnet_data:rmnet_unregister_cb_unhandled");
      if (rmnet_data.has_rmnet_vnd_start_xmit() && rmnet_data.rmnet_vnd_start_xmit())
        result.push_back("rmnet_data:rmnet_vnd_start_xmit");
    }

    if (tracepoints.has_rndis_ipa()) {
      const auto& rndis_ipa = tracepoints.rndis_ipa();

      if (rndis_ipa.has_rndis_netif_ni() && rndis_ipa.rndis_netif_ni())
        result.push_back("rndis_ipa:rndis_netif_ni");
      if (rndis_ipa.has_rndis_status_rcvd() && rndis_ipa.rndis_status_rcvd())
        result.push_back("rndis_ipa:rndis_status_rcvd");
      if (rndis_ipa.has_rndis_tx_dp() && rndis_ipa.rndis_tx_dp())
        result.push_back("rndis_ipa:rndis_tx_dp");
    }

    if (tracepoints.has_rpm()) {
      const auto& rpm = tracepoints.rpm();

      if (rpm.has_rpm_idle() && rpm.rpm_idle())
        result.push_back("rpm:rpm_idle");
      if (rpm.has_rpm_resume() && rpm.rpm_resume())
        result.push_back("rpm:rpm_resume");
      if (rpm.has_rpm_return_int() && rpm.rpm_return_int())
        result.push_back("rpm:rpm_return_int");
      if (rpm.has_rpm_suspend() && rpm.rpm_suspend())
        result.push_back("rpm:rpm_suspend");
    }

    if (tracepoints.has_rpm_smd()) {
      const auto& rpm_smd = tracepoints.rpm_smd();

      if (rpm_smd.has_rpm_smd_ack_recvd() && rpm_smd.rpm_smd_ack_recvd())
        result.push_back("rpm_smd:rpm_smd_ack_recvd");
      if (rpm_smd.has_rpm_smd_interrupt_notify() && rpm_smd.rpm_smd_interrupt_notify())
        result.push_back("rpm_smd:rpm_smd_interrupt_notify");
      if (rpm_smd.has_rpm_smd_send_active_set() && rpm_smd.rpm_smd_send_active_set())
        result.push_back("rpm_smd:rpm_smd_send_active_set");
      if (rpm_smd.has_rpm_smd_send_sleep_set() && rpm_smd.rpm_smd_send_sleep_set())
        result.push_back("rpm_smd:rpm_smd_send_sleep_set");
      if (rpm_smd.has_rpm_smd_sleep_set() && rpm_smd.rpm_smd_sleep_set())
        result.push_back("rpm_smd:rpm_smd_sleep_set");
    }

    if (tracepoints.has_sched()) {
      const auto& sched = tracepoints.sched();

      if (sched.has_sched_blocked_reason() && sched.sched_blocked_reason())
        result.push_back("sched:sched_blocked_reason");
      if (sched.has_sched_boost_cpu() && sched.sched_boost_cpu())
        result.push_back("sched:sched_boost_cpu");
      if (sched.has_sched_boost_task() && sched.sched_boost_task())
        result.push_back("sched:sched_boost_task");
      if (sched.has_sched_contrib_scale_f() && sched.sched_contrib_scale_f())
        result.push_back("sched:sched_contrib_scale_f");
      if (sched.has_sched_energy_diff() && sched.sched_energy_diff())
        result.push_back("sched:sched_energy_diff");
      if (sched.has_sched_energy_perf_deltas() && sched.sched_energy_perf_deltas())
        result.push_back("sched:sched_energy_perf_deltas");
      if (sched.has_sched_kthread_stop() && sched.sched_kthread_stop())
        result.push_back("sched:sched_kthread_stop");
      if (sched.has_sched_kthread_stop_ret() && sched.sched_kthread_stop_ret())
        result.push_back("sched:sched_kthread_stop_ret");
      if (sched.has_sched_load_avg_cpu() && sched.sched_load_avg_cpu())
        result.push_back("sched:sched_load_avg_cpu");
      if (sched.has_sched_load_avg_task() && sched.sched_load_avg_task())
        result.push_back("sched:sched_load_avg_task");
      if (sched.has_sched_migrate_task() && sched.sched_migrate_task())
        result.push_back("sched:sched_migrate_task");
      if (sched.has_sched_move_numa() && sched.sched_move_numa())
        result.push_back("sched:sched_move_numa");
      if (sched.has_sched_overutilized() && sched.sched_overutilized())
        result.push_back("sched:sched_overutilized");
      if (sched.has_sched_pi_setprio() && sched.sched_pi_setprio())
        result.push_back("sched:sched_pi_setprio");
      if (sched.has_sched_process_exec() && sched.sched_process_exec())
        result.push_back("sched:sched_process_exec");
      if (sched.has_sched_process_exit() && sched.sched_process_exit())
        result.push_back("sched:sched_process_exit");
      if (sched.has_sched_process_fork() && sched.sched_process_fork())
        result.push_back("sched:sched_process_fork");
      if (sched.has_sched_process_free() && sched.sched_process_free())
        result.push_back("sched:sched_process_free");
      if (sched.has_sched_process_hang() && sched.sched_process_hang())
        result.push_back("sched:sched_process_hang");
      if (sched.has_sched_process_wait() && sched.sched_process_wait())
        result.push_back("sched:sched_process_wait");
      if (sched.has_sched_stat_blocked() && sched.sched_stat_blocked())
        result.push_back("sched:sched_stat_blocked");
      if (sched.has_sched_stat_iowait() && sched.sched_stat_iowait())
        result.push_back("sched:sched_stat_iowait");
      if (sched.has_sched_stat_runtime() && sched.sched_stat_runtime())
        result.push_back("sched:sched_stat_runtime");
      if (sched.has_sched_stat_sleep() && sched.sched_stat_sleep())
        result.push_back("sched:sched_stat_sleep");
      if (sched.has_sched_stat_wait() && sched.sched_stat_wait())
        result.push_back("sched:sched_stat_wait");
      if (sched.has_sched_stick_numa() && sched.sched_stick_numa())
        result.push_back("sched:sched_stick_numa");
      if (sched.has_sched_swap_numa() && sched.sched_swap_numa())
        result.push_back("sched:sched_swap_numa");
      if (sched.has_sched_switch() && sched.sched_switch())
        result.push_back("sched:sched_switch");
      if (sched.has_sched_tune_boostgroup_update() && sched.sched_tune_boostgroup_update())
        result.push_back("sched:sched_tune_boostgroup_update");
      if (sched.has_sched_tune_config() && sched.sched_tune_config())
        result.push_back("sched:sched_tune_config");
      if (sched.has_sched_tune_filter() && sched.sched_tune_filter())
        result.push_back("sched:sched_tune_filter");
      if (sched.has_sched_tune_tasks_update() && sched.sched_tune_tasks_update())
        result.push_back("sched:sched_tune_tasks_update");
      if (sched.has_sched_wait_task() && sched.sched_wait_task())
        result.push_back("sched:sched_wait_task");
      if (sched.has_sched_wake_idle_without_ipi() && sched.sched_wake_idle_without_ipi())
        result.push_back("sched:sched_wake_idle_without_ipi");
      if (sched.has_sched_wakeup() && sched.sched_wakeup())
        result.push_back("sched:sched_wakeup");
      if (sched.has_sched_wakeup_new() && sched.sched_wakeup_new())
        result.push_back("sched:sched_wakeup_new");
      if (sched.has_sched_waking() && sched.sched_waking())
        result.push_back("sched:sched_waking");
      if (sched.has_walt_migration_update_sum() && sched.walt_migration_update_sum())
        result.push_back("sched:walt_migration_update_sum");
      if (sched.has_walt_update_history() && sched.walt_update_history())
        result.push_back("sched:walt_update_history");
      if (sched.has_walt_update_task_ravg() && sched.walt_update_task_ravg())
        result.push_back("sched:walt_update_task_ravg");
    }

    if (tracepoints.has_scm()) {
      const auto& scm = tracepoints.scm();

      if (scm.has_scm_call_end() && scm.scm_call_end())
        result.push_back("scm:scm_call_end");
      if (scm.has_scm_call_start() && scm.scm_call_start())
        result.push_back("scm:scm_call_start");
    }

    if (tracepoints.has_scsi()) {
      const auto& scsi = tracepoints.scsi();

      if (scsi.has_scsi_dispatch_cmd_done() && scsi.scsi_dispatch_cmd_done())
        result.push_back("scsi:scsi_dispatch_cmd_done");
      if (scsi.has_scsi_dispatch_cmd_error() && scsi.scsi_dispatch_cmd_error())
        result.push_back("scsi:scsi_dispatch_cmd_error");
      if (scsi.has_scsi_dispatch_cmd_start() && scsi.scsi_dispatch_cmd_start())
        result.push_back("scsi:scsi_dispatch_cmd_start");
      if (scsi.has_scsi_dispatch_cmd_timeout() && scsi.scsi_dispatch_cmd_timeout())
        result.push_back("scsi:scsi_dispatch_cmd_timeout");
      if (scsi.has_scsi_eh_wakeup() && scsi.scsi_eh_wakeup())
        result.push_back("scsi:scsi_eh_wakeup");
    }

    if (tracepoints.has_signal()) {
      const auto& signal = tracepoints.signal();

      if (signal.has_signal_deliver() && signal.signal_deliver())
        result.push_back("signal:signal_deliver");
      if (signal.has_signal_generate() && signal.signal_generate())
        result.push_back("signal:signal_generate");
    }

    if (tracepoints.has_skb()) {
      const auto& skb = tracepoints.skb();

      if (skb.has_consume_skb() && skb.consume_skb())
        result.push_back("skb:consume_skb");
      if (skb.has_kfree_skb() && skb.kfree_skb())
        result.push_back("skb:kfree_skb");
      if (skb.has_print_skb_gso() && skb.print_skb_gso())
        result.push_back("skb:print_skb_gso");
      if (skb.has_skb_copy_datagram_iovec() && skb.skb_copy_datagram_iovec())
        result.push_back("skb:skb_copy_datagram_iovec");
    }

    if (tracepoints.has_sock()) {
      const auto& sock = tracepoints.sock();

      if (sock.has_sock_exceed_buf_limit() && sock.sock_exceed_buf_limit())
        result.push_back("sock:sock_exceed_buf_limit");
      if (sock.has_sock_rcvqueue_full() && sock.sock_rcvqueue_full())
        result.push_back("sock:sock_rcvqueue_full");
    }

    if (tracepoints.has_spi()) {
      const auto& spi = tracepoints.spi();

      if (spi.has_spi_master_busy() && spi.spi_master_busy())
        result.push_back("spi:spi_master_busy");
      if (spi.has_spi_master_idle() && spi.spi_master_idle())
        result.push_back("spi:spi_master_idle");
      if (spi.has_spi_message_done() && spi.spi_message_done())
        result.push_back("spi:spi_message_done");
      if (spi.has_spi_message_start() && spi.spi_message_start())
        result.push_back("spi:spi_message_start");
      if (spi.has_spi_message_submit() && spi.spi_message_submit())
        result.push_back("spi:spi_message_submit");
      if (spi.has_spi_transfer_start() && spi.spi_transfer_start())
        result.push_back("spi:spi_transfer_start");
      if (spi.has_spi_transfer_stop() && spi.spi_transfer_stop())
        result.push_back("spi:spi_transfer_stop");
    }

    if (tracepoints.has_swiotlb()) {
      const auto& swiotlb = tracepoints.swiotlb();

      if (swiotlb.has_swiotlb_bounced() && swiotlb.swiotlb_bounced())
        result.push_back("swiotlb:swiotlb_bounced");
    }

    if (tracepoints.has_sync()) {
      const auto& sync = tracepoints.sync();

      if (sync.has_sync_pt() && sync.sync_pt())
        result.push_back("sync:sync_pt");
      if (sync.has_sync_timeline() && sync.sync_timeline())
        result.push_back("sync:sync_timeline");
      if (sync.has_sync_wait() && sync.sync_wait())
        result.push_back("sync:sync_wait");
    }

    if (tracepoints.has_task()) {
      const auto& task = tracepoints.task();

      if (task.has_task_newtask() && task.task_newtask())
        result.push_back("task:task_newtask");
      if (task.has_task_rename() && task.task_rename())
        result.push_back("task:task_rename");
    }

    if (tracepoints.has_thermal()) {
      const auto& thermal = tracepoints.thermal();

      if (thermal.has_bcl_hw_event() && thermal.bcl_hw_event())
        result.push_back("thermal:bcl_hw_event");
      if (thermal.has_bcl_hw_mitigation() && thermal.bcl_hw_mitigation())
        result.push_back("thermal:bcl_hw_mitigation");
      if (thermal.has_bcl_hw_mitigation_event() && thermal.bcl_hw_mitigation_event())
        result.push_back("thermal:bcl_hw_mitigation_event");
      if (thermal.has_bcl_hw_reg_access() && thermal.bcl_hw_reg_access())
        result.push_back("thermal:bcl_hw_reg_access");
      if (thermal.has_bcl_hw_sensor_reading() && thermal.bcl_hw_sensor_reading())
        result.push_back("thermal:bcl_hw_sensor_reading");
      if (thermal.has_bcl_hw_state_event() && thermal.bcl_hw_state_event())
        result.push_back("thermal:bcl_hw_state_event");
      if (thermal.has_bcl_sw_mitigation() && thermal.bcl_sw_mitigation())
        result.push_back("thermal:bcl_sw_mitigation");
      if (thermal.has_bcl_sw_mitigation_event() && thermal.bcl_sw_mitigation_event())
        result.push_back("thermal:bcl_sw_mitigation_event");
      if (thermal.has_cdev_update() && thermal.cdev_update())
        result.push_back("thermal:cdev_update");
      if (thermal.has_thermal_post_core_offline() && thermal.thermal_post_core_offline())
        result.push_back("thermal:thermal_post_core_offline");
      if (thermal.has_thermal_post_core_online() && thermal.thermal_post_core_online())
        result.push_back("thermal:thermal_post_core_online");
      if (thermal.has_thermal_post_frequency_mit() && thermal.thermal_post_frequency_mit())
        result.push_back("thermal:thermal_post_frequency_mit");
      if (thermal.has_thermal_power_cpu_get_power() && thermal.thermal_power_cpu_get_power())
        result.push_back("thermal:thermal_power_cpu_get_power");
      if (thermal.has_thermal_power_cpu_limit() && thermal.thermal_power_cpu_limit())
        result.push_back("thermal:thermal_power_cpu_limit");
      if (thermal.has_thermal_pre_core_offline() && thermal.thermal_pre_core_offline())
        result.push_back("thermal:thermal_pre_core_offline");
      if (thermal.has_thermal_pre_core_online() && thermal.thermal_pre_core_online())
        result.push_back("thermal:thermal_pre_core_online");
      if (thermal.has_thermal_pre_frequency_mit() && thermal.thermal_pre_frequency_mit())
        result.push_back("thermal:thermal_pre_frequency_mit");
      if (thermal.has_thermal_temperature() && thermal.thermal_temperature())
        result.push_back("thermal:thermal_temperature");
      if (thermal.has_thermal_zone_trip() && thermal.thermal_zone_trip())
        result.push_back("thermal:thermal_zone_trip");
      if (thermal.has_tsens_read() && thermal.tsens_read())
        result.push_back("thermal:tsens_read");
      if (thermal.has_tsens_threshold_clear() && thermal.tsens_threshold_clear())
        result.push_back("thermal:tsens_threshold_clear");
      if (thermal.has_tsens_threshold_hit() && thermal.tsens_threshold_hit())
        result.push_back("thermal:tsens_threshold_hit");
    }

    if (tracepoints.has_timer()) {
      const auto& timer = tracepoints.timer();

      if (timer.has_hrtimer_cancel() && timer.hrtimer_cancel())
        result.push_back("timer:hrtimer_cancel");
      if (timer.has_hrtimer_expire_entry() && timer.hrtimer_expire_entry())
        result.push_back("timer:hrtimer_expire_entry");
      if (timer.has_hrtimer_expire_exit() && timer.hrtimer_expire_exit())
        result.push_back("timer:hrtimer_expire_exit");
      if (timer.has_hrtimer_init() && timer.hrtimer_init())
        result.push_back("timer:hrtimer_init");
      if (timer.has_hrtimer_start() && timer.hrtimer_start())
        result.push_back("timer:hrtimer_start");
      if (timer.has_itimer_expire() && timer.itimer_expire())
        result.push_back("timer:itimer_expire");
      if (timer.has_itimer_state() && timer.itimer_state())
        result.push_back("timer:itimer_state");
      if (timer.has_tick_stop() && timer.tick_stop())
        result.push_back("timer:tick_stop");
      if (timer.has_timer_cancel() && timer.timer_cancel())
        result.push_back("timer:timer_cancel");
      if (timer.has_timer_expire_entry() && timer.timer_expire_entry())
        result.push_back("timer:timer_expire_entry");
      if (timer.has_timer_expire_exit() && timer.timer_expire_exit())
        result.push_back("timer:timer_expire_exit");
      if (timer.has_timer_init() && timer.timer_init())
        result.push_back("timer:timer_init");
      if (timer.has_timer_start() && timer.timer_start())
        result.push_back("timer:timer_start");
    }

    if (tracepoints.has_tracer_pkt()) {
      const auto& tracer_pkt = tracepoints.tracer_pkt();

      if (tracer_pkt.has_tracer_pkt_event() && tracer_pkt.tracer_pkt_event())
        result.push_back("tracer_pkt:tracer_pkt_event");
    }

    if (tracepoints.has_udp()) {
      const auto& udp = tracepoints.udp();

      if (udp.has_udp_fail_queue_rcv_skb() && udp.udp_fail_queue_rcv_skb())
        result.push_back("udp:udp_fail_queue_rcv_skb");
    }

    if (tracepoints.has_ufs()) {
      const auto& ufs = tracepoints.ufs();

      if (ufs.has_ufshcd_auto_bkops_state() && ufs.ufshcd_auto_bkops_state())
        result.push_back("ufs:ufshcd_auto_bkops_state");
      if (ufs.has_ufshcd_clk_gating() && ufs.ufshcd_clk_gating())
        result.push_back("ufs:ufshcd_clk_gating");
      if (ufs.has_ufshcd_clk_scaling() && ufs.ufshcd_clk_scaling())
        result.push_back("ufs:ufshcd_clk_scaling");
      if (ufs.has_ufshcd_command() && ufs.ufshcd_command())
        result.push_back("ufs:ufshcd_command");
      if (ufs.has_ufshcd_hibern8_on_idle() && ufs.ufshcd_hibern8_on_idle())
        result.push_back("ufs:ufshcd_hibern8_on_idle");
      if (ufs.has_ufshcd_init() && ufs.ufshcd_init())
        result.push_back("ufs:ufshcd_init");
      if (ufs.has_ufshcd_profile_clk_gating() && ufs.ufshcd_profile_clk_gating())
        result.push_back("ufs:ufshcd_profile_clk_gating");
      if (ufs.has_ufshcd_profile_clk_scaling() && ufs.ufshcd_profile_clk_scaling())
        result.push_back("ufs:ufshcd_profile_clk_scaling");
      if (ufs.has_ufshcd_profile_hibern8() && ufs.ufshcd_profile_hibern8())
        result.push_back("ufs:ufshcd_profile_hibern8");
      if (ufs.has_ufshcd_runtime_resume() && ufs.ufshcd_runtime_resume())
        result.push_back("ufs:ufshcd_runtime_resume");
      if (ufs.has_ufshcd_runtime_suspend() && ufs.ufshcd_runtime_suspend())
        result.push_back("ufs:ufshcd_runtime_suspend");
      if (ufs.has_ufshcd_system_resume() && ufs.ufshcd_system_resume())
        result.push_back("ufs:ufshcd_system_resume");
      if (ufs.has_ufshcd_system_suspend() && ufs.ufshcd_system_suspend())
        result.push_back("ufs:ufshcd_system_suspend");
    }

    if (tracepoints.has_v4l2()) {
      const auto& v4l2 = tracepoints.v4l2();

      if (v4l2.has_v4l2_dqbuf() && v4l2.v4l2_dqbuf())
        result.push_back("v4l2:v4l2_dqbuf");
      if (v4l2.has_v4l2_qbuf() && v4l2.v4l2_qbuf())
        result.push_back("v4l2:v4l2_qbuf");
    }

    if (tracepoints.has_vmscan()) {
      const auto& vmscan = tracepoints.vmscan();

      if (vmscan.has_mm_shrink_slab_end() && vmscan.mm_shrink_slab_end())
        result.push_back("vmscan:mm_shrink_slab_end");
      if (vmscan.has_mm_shrink_slab_start() && vmscan.mm_shrink_slab_start())
        result.push_back("vmscan:mm_shrink_slab_start");
      if (vmscan.has_mm_vmscan_direct_reclaim_begin() && vmscan.mm_vmscan_direct_reclaim_begin())
        result.push_back("vmscan:mm_vmscan_direct_reclaim_begin");
      if (vmscan.has_mm_vmscan_direct_reclaim_end() && vmscan.mm_vmscan_direct_reclaim_end())
        result.push_back("vmscan:mm_vmscan_direct_reclaim_end");
      if (vmscan.has_mm_vmscan_kswapd_sleep() && vmscan.mm_vmscan_kswapd_sleep())
        result.push_back("vmscan:mm_vmscan_kswapd_sleep");
      if (vmscan.has_mm_vmscan_kswapd_wake() && vmscan.mm_vmscan_kswapd_wake())
        result.push_back("vmscan:mm_vmscan_kswapd_wake");
      if (vmscan.has_mm_vmscan_lru_isolate() && vmscan.mm_vmscan_lru_isolate())
        result.push_back("vmscan:mm_vmscan_lru_isolate");
      if (vmscan.has_mm_vmscan_lru_shrink_inactive() && vmscan.mm_vmscan_lru_shrink_inactive())
        result.push_back("vmscan:mm_vmscan_lru_shrink_inactive");
      if (vmscan.has_mm_vmscan_memcg_isolate() && vmscan.mm_vmscan_memcg_isolate())
        result.push_back("vmscan:mm_vmscan_memcg_isolate");
      if (vmscan.has_mm_vmscan_memcg_reclaim_begin() && vmscan.mm_vmscan_memcg_reclaim_begin())
        result.push_back("vmscan:mm_vmscan_memcg_reclaim_begin");
      if (vmscan.has_mm_vmscan_memcg_reclaim_end() && vmscan.mm_vmscan_memcg_reclaim_end())
        result.push_back("vmscan:mm_vmscan_memcg_reclaim_end");
      if (vmscan.has_mm_vmscan_memcg_softlimit_reclaim_begin() && vmscan.mm_vmscan_memcg_softlimit_reclaim_begin())
        result.push_back("vmscan:mm_vmscan_memcg_softlimit_reclaim_begin");
      if (vmscan.has_mm_vmscan_memcg_softlimit_reclaim_end() && vmscan.mm_vmscan_memcg_softlimit_reclaim_end())
        result.push_back("vmscan:mm_vmscan_memcg_softlimit_reclaim_end");
      if (vmscan.has_mm_vmscan_wakeup_kswapd() && vmscan.mm_vmscan_wakeup_kswapd())
        result.push_back("vmscan:mm_vmscan_wakeup_kswapd");
      if (vmscan.has_mm_vmscan_writepage() && vmscan.mm_vmscan_writepage())
        result.push_back("vmscan:mm_vmscan_writepage");
    }

    if (tracepoints.has_workqueue()) {
      const auto& workqueue = tracepoints.workqueue();

      if (workqueue.has_workqueue_activate_work() && workqueue.workqueue_activate_work())
        result.push_back("workqueue:workqueue_activate_work");
      if (workqueue.has_workqueue_execute_end() && workqueue.workqueue_execute_end())
        result.push_back("workqueue:workqueue_execute_end");
      if (workqueue.has_workqueue_execute_start() && workqueue.workqueue_execute_start())
        result.push_back("workqueue:workqueue_execute_start");
      if (workqueue.has_workqueue_queue_work() && workqueue.workqueue_queue_work())
        result.push_back("workqueue:workqueue_queue_work");
    }

    if (tracepoints.has_writeback()) {
      const auto& writeback = tracepoints.writeback();

      if (writeback.has_balance_dirty_pages() && writeback.balance_dirty_pages())
        result.push_back("writeback:balance_dirty_pages");
      if (writeback.has_bdi_dirty_ratelimit() && writeback.bdi_dirty_ratelimit())
        result.push_back("writeback:bdi_dirty_ratelimit");
      if (writeback.has_global_dirty_state() && writeback.global_dirty_state())
        result.push_back("writeback:global_dirty_state");
      if (writeback.has_wbc_writepage() && writeback.wbc_writepage())
        result.push_back("writeback:wbc_writepage");
      if (writeback.has_writeback_bdi_register() && writeback.writeback_bdi_register())
        result.push_back("writeback:writeback_bdi_register");
      if (writeback.has_writeback_bdi_unregister() && writeback.writeback_bdi_unregister())
        result.push_back("writeback:writeback_bdi_unregister");
      if (writeback.has_writeback_congestion_wait() && writeback.writeback_congestion_wait())
        result.push_back("writeback:writeback_congestion_wait");
      if (writeback.has_writeback_dirty_inode() && writeback.writeback_dirty_inode())
        result.push_back("writeback:writeback_dirty_inode");
      if (writeback.has_writeback_dirty_inode_start() && writeback.writeback_dirty_inode_start())
        result.push_back("writeback:writeback_dirty_inode_start");
      if (writeback.has_writeback_dirty_page() && writeback.writeback_dirty_page())
        result.push_back("writeback:writeback_dirty_page");
      if (writeback.has_writeback_exec() && writeback.writeback_exec())
        result.push_back("writeback:writeback_exec");
      if (writeback.has_writeback_nowork() && writeback.writeback_nowork())
        result.push_back("writeback:writeback_nowork");
      if (writeback.has_writeback_pages_written() && writeback.writeback_pages_written())
        result.push_back("writeback:writeback_pages_written");
      if (writeback.has_writeback_queue() && writeback.writeback_queue())
        result.push_back("writeback:writeback_queue");
      if (writeback.has_writeback_queue_io() && writeback.writeback_queue_io())
        result.push_back("writeback:writeback_queue_io");
      if (writeback.has_writeback_sb_inodes_requeue() && writeback.writeback_sb_inodes_requeue())
        result.push_back("writeback:writeback_sb_inodes_requeue");
      if (writeback.has_writeback_single_inode() && writeback.writeback_single_inode())
        result.push_back("writeback:writeback_single_inode");
      if (writeback.has_writeback_single_inode_start() && writeback.writeback_single_inode_start())
        result.push_back("writeback:writeback_single_inode_start");
      if (writeback.has_writeback_start() && writeback.writeback_start())
        result.push_back("writeback:writeback_start");
      if (writeback.has_writeback_wait() && writeback.writeback_wait())
        result.push_back("writeback:writeback_wait");
      if (writeback.has_writeback_wait_iff_congested() && writeback.writeback_wait_iff_congested())
        result.push_back("writeback:writeback_wait_iff_congested");
      if (writeback.has_writeback_wake_background() && writeback.writeback_wake_background())
        result.push_back("writeback:writeback_wake_background");
      if (writeback.has_writeback_write_inode() && writeback.writeback_write_inode())
        result.push_back("writeback:writeback_write_inode");
      if (writeback.has_writeback_write_inode_start() && writeback.writeback_write_inode_start())
        result.push_back("writeback:writeback_write_inode_start");
      if (writeback.has_writeback_written() && writeback.writeback_written())
        result.push_back("writeback:writeback_written");
    }

    if (tracepoints.has_xhci_hcd()) {
      const auto& xhci_hcd = tracepoints.xhci_hcd();

      if (xhci_hcd.has_xhci_address_ctx() && xhci_hcd.xhci_address_ctx())
        result.push_back("xhci-hcd:xhci_address_ctx");
      if (xhci_hcd.has_xhci_cmd_completion() && xhci_hcd.xhci_cmd_completion())
        result.push_back("xhci-hcd:xhci_cmd_completion");
      if (xhci_hcd.has_xhci_dbg_address() && xhci_hcd.xhci_dbg_address())
        result.push_back("xhci-hcd:xhci_dbg_address");
      if (xhci_hcd.has_xhci_dbg_cancel_urb() && xhci_hcd.xhci_dbg_cancel_urb())
        result.push_back("xhci-hcd:xhci_dbg_cancel_urb");
      if (xhci_hcd.has_xhci_dbg_context_change() && xhci_hcd.xhci_dbg_context_change())
        result.push_back("xhci-hcd:xhci_dbg_context_change");
      if (xhci_hcd.has_xhci_dbg_init() && xhci_hcd.xhci_dbg_init())
        result.push_back("xhci-hcd:xhci_dbg_init");
      if (xhci_hcd.has_xhci_dbg_quirks() && xhci_hcd.xhci_dbg_quirks())
        result.push_back("xhci-hcd:xhci_dbg_quirks");
      if (xhci_hcd.has_xhci_dbg_reset_ep() && xhci_hcd.xhci_dbg_reset_ep())
        result.push_back("xhci-hcd:xhci_dbg_reset_ep");
      if (xhci_hcd.has_xhci_dbg_ring_expansion() && xhci_hcd.xhci_dbg_ring_expansion())
        result.push_back("xhci-hcd:xhci_dbg_ring_expansion");
    }

  }

  return result;
}

}  // namespace perfprofd
}  // namespace android
