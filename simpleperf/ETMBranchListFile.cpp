/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include "ETMBranchListFile.h"

#include "ETMDecoder.h"
#include "system/extras/simpleperf/etm_branch_list.pb.h"

namespace simpleperf {

static constexpr const char* ETM_BRANCH_LIST_PROTO_MAGIC = "simpleperf:EtmBranchList";

std::string BranchToProtoString(const std::vector<bool>& branch) {
  size_t bytes = (branch.size() + 7) / 8;
  std::string res(bytes, '\0');
  for (size_t i = 0; i < branch.size(); i++) {
    if (branch[i]) {
      res[i >> 3] |= 1 << (i & 7);
    }
  }
  return res;
}

std::vector<bool> ProtoStringToBranch(const std::string& s, size_t bit_size) {
  std::vector<bool> branch(bit_size, false);
  for (size_t i = 0; i < bit_size; i++) {
    if (s[i >> 3] & (1 << (i & 7))) {
      branch[i] = true;
    }
  }
  return branch;
}

static std::optional<proto::ETMBranchList_Binary::BinaryType> ToProtoBinaryType(DsoType dso_type) {
  switch (dso_type) {
    case DSO_ELF_FILE:
      return proto::ETMBranchList_Binary::ELF_FILE;
    case DSO_KERNEL:
      return proto::ETMBranchList_Binary::KERNEL;
    case DSO_KERNEL_MODULE:
      return proto::ETMBranchList_Binary::KERNEL_MODULE;
    default:
      LOG(ERROR) << "unexpected dso type " << dso_type;
      return std::nullopt;
  }
}

bool BranchListBinaryMapToString(const BranchListBinaryMap& binary_map, std::string& s) {
  proto::ETMBranchList branch_list_proto;
  branch_list_proto.set_magic(ETM_BRANCH_LIST_PROTO_MAGIC);
  std::vector<char> branch_buf;
  for (const auto& p : binary_map) {
    const BinaryKey& key = p.first;
    const BranchListBinaryInfo& binary = p.second;
    auto binary_proto = branch_list_proto.add_binaries();

    binary_proto->set_path(key.path);
    if (!key.build_id.IsEmpty()) {
      binary_proto->set_build_id(key.build_id.ToString().substr(2));
    }
    auto opt_binary_type = ToProtoBinaryType(binary.dso_type);
    if (!opt_binary_type.has_value()) {
      return false;
    }
    binary_proto->set_type(opt_binary_type.value());

    for (const auto& addr_p : binary.branch_map) {
      auto addr_proto = binary_proto->add_addrs();
      addr_proto->set_addr(addr_p.first);

      for (const auto& branch_p : addr_p.second) {
        const std::vector<bool>& branch = branch_p.first;
        auto branch_proto = addr_proto->add_branches();

        branch_proto->set_branch(BranchToProtoString(branch));
        branch_proto->set_branch_size(branch.size());
        branch_proto->set_count(branch_p.second);
      }
    }

    if (binary.dso_type == DSO_KERNEL) {
      binary_proto->mutable_kernel_info()->set_kernel_start_addr(key.kernel_start_addr);
    }
  }
  if (!branch_list_proto.SerializeToString(&s)) {
    LOG(ERROR) << "failed to serialize branch list binary map";
    return false;
  }
  return true;
}

static std::optional<DsoType> ToDsoType(proto::ETMBranchList_Binary::BinaryType binary_type) {
  switch (binary_type) {
    case proto::ETMBranchList_Binary::ELF_FILE:
      return DSO_ELF_FILE;
    case proto::ETMBranchList_Binary::KERNEL:
      return DSO_KERNEL;
    case proto::ETMBranchList_Binary::KERNEL_MODULE:
      return DSO_KERNEL_MODULE;
    default:
      LOG(ERROR) << "unexpected binary type " << binary_type;
      return std::nullopt;
  }
}

static UnorderedBranchMap BuildUnorderedBranchMap(const proto::ETMBranchList_Binary& binary_proto) {
  UnorderedBranchMap branch_map;
  for (size_t i = 0; i < binary_proto.addrs_size(); i++) {
    const auto& addr_proto = binary_proto.addrs(i);
    auto& b_map = branch_map[addr_proto.addr()];
    for (size_t j = 0; j < addr_proto.branches_size(); j++) {
      const auto& branch_proto = addr_proto.branches(j);
      std::vector<bool> branch =
          ProtoStringToBranch(branch_proto.branch(), branch_proto.branch_size());
      b_map[branch] = branch_proto.count();
    }
  }
  return branch_map;
}

bool StringToBranchListBinaryMap(const std::string& s, BranchListBinaryMap& binary_map) {
  proto::ETMBranchList branch_list_proto;
  if (!branch_list_proto.ParseFromString(s)) {
    PLOG(ERROR) << "failed to read ETMBranchList msg";
    return false;
  }
  if (branch_list_proto.magic() != ETM_BRANCH_LIST_PROTO_MAGIC) {
    PLOG(ERROR) << "not in format etm_branch_list.proto";
    return false;
  }

  for (size_t i = 0; i < branch_list_proto.binaries_size(); i++) {
    const auto& binary_proto = branch_list_proto.binaries(i);
    BinaryKey key(binary_proto.path(), BuildId(binary_proto.build_id()));
    if (binary_proto.has_kernel_info()) {
      key.kernel_start_addr = binary_proto.kernel_info().kernel_start_addr();
    }
    BranchListBinaryInfo& binary = binary_map[key];
    auto dso_type = ToDsoType(binary_proto.type());
    if (!dso_type) {
      LOG(ERROR) << "invalid binary type " << binary_proto.type();
      return false;
    }
    binary.dso_type = dso_type.value();
    binary.branch_map = BuildUnorderedBranchMap(binary_proto);
  }
  return true;
}

}  // namespace simpleperf
