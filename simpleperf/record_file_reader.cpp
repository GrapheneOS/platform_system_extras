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

#include "record_file.h"

#include <fcntl.h>
#include <string.h>
#include <set>
#include <vector>

#include <android-base/logging.h>

#include "event_attr.h"
#include "record.h"
#include "utils.h"

using namespace PerfFileFormat;

std::unique_ptr<RecordFileReader> RecordFileReader::CreateInstance(const std::string& filename) {
  std::string mode = std::string("rb") + CLOSE_ON_EXEC_MODE;
  FILE* fp = fopen(filename.c_str(), mode.c_str());
  if (fp == nullptr) {
    PLOG(ERROR) << "failed to open record file '" << filename << "'";
    return nullptr;
  }
  auto reader = std::unique_ptr<RecordFileReader>(new RecordFileReader(filename, fp));
  if (!reader->ReadHeader() || !reader->ReadAttrSection() ||
      !reader->ReadFeatureSectionDescriptors()) {
    return nullptr;
  }
  return reader;
}

RecordFileReader::RecordFileReader(const std::string& filename, FILE* fp)
    : filename_(filename), record_fp_(fp), event_id_pos_in_sample_records_(0),
      event_id_reverse_pos_in_non_sample_records_(0) {
}

RecordFileReader::~RecordFileReader() {
  if (record_fp_ != nullptr) {
    Close();
  }
}

bool RecordFileReader::Close() {
  bool result = true;
  if (fclose(record_fp_) != 0) {
    PLOG(ERROR) << "failed to close record file '" << filename_ << "'";
    result = false;
  }
  record_fp_ = nullptr;
  return result;
}

bool RecordFileReader::ReadHeader() {
  return Read(&header_, sizeof(header_));
}

bool RecordFileReader::ReadAttrSection() {
  size_t attr_count = header_.attrs.size / header_.attr_size;
  if (header_.attr_size != sizeof(FileAttr)) {
    LOG(DEBUG) << "attr size (" << header_.attr_size << ") in " << filename_
                 << " doesn't match expected size (" << sizeof(FileAttr) << ")";
  }
  if (attr_count == 0) {
    LOG(ERROR) << "no attr in file " << filename_;
    return false;
  }
  if (fseek(record_fp_, header_.attrs.offset, SEEK_SET) != 0) {
    PLOG(ERROR) << "fseek() failed";
    return false;
  }
  for (size_t i = 0; i < attr_count; ++i) {
    std::vector<char> buf(header_.attr_size);
    if (!Read(buf.data(), buf.size())) {
      return false;
    }
    // The size of perf_event_attr is changing between different linux kernel versions.
    // Make sure we copy correct data to memory.
    FileAttr attr;
    memset(&attr, 0, sizeof(attr));
    size_t section_desc_size = sizeof(attr.ids);
    size_t perf_event_attr_size = header_.attr_size - section_desc_size;
    memcpy(&attr.attr, &buf[0], std::min(sizeof(attr.attr), perf_event_attr_size));
    memcpy(&attr.ids, &buf[perf_event_attr_size], section_desc_size);
    file_attrs_.push_back(attr);
  }
  if (file_attrs_.size() > 1) {
    std::vector<perf_event_attr> attrs;
    for (const auto& file_attr : file_attrs_) {
      attrs.push_back(file_attr.attr);
    }
    if (!GetCommonEventIdPositionsForAttrs(attrs, &event_id_pos_in_sample_records_,
                                               &event_id_reverse_pos_in_non_sample_records_)) {
      return false;
    }
  }
  for (size_t i = 0; i < file_attrs_.size(); ++i) {
    std::vector<uint64_t> ids;
    if (!ReadIdsForAttr(file_attrs_[i], &ids)) {
      return false;
    }
    event_ids_for_file_attrs_.push_back(ids);
    for (auto id : ids) {
      event_id_to_attr_map_[id] = &file_attrs_[i].attr;
    }
  }
  return true;
}

bool RecordFileReader::ReadFeatureSectionDescriptors() {
  std::vector<int> features;
  for (size_t i = 0; i < sizeof(header_.features); ++i) {
    for (size_t j = 0; j < 8; ++j) {
      if (header_.features[i] & (1 << j)) {
        features.push_back(i * 8 + j);
      }
    }
  }
  uint64_t feature_section_offset = header_.data.offset + header_.data.size;
  if (fseek(record_fp_, feature_section_offset, SEEK_SET) != 0) {
    PLOG(ERROR) << "fseek() failed";
    return false;
  }
  for (const auto& id : features) {
    SectionDesc desc;
    if (!Read(&desc, sizeof(desc))) {
      return false;
    }
    feature_section_descriptors_.emplace(id, desc);
  }
  return true;
}

bool RecordFileReader::ReadIdsForAttr(const FileAttr& attr, std::vector<uint64_t>* ids) {
  size_t id_count = attr.ids.size / sizeof(uint64_t);
  if (fseek(record_fp_, attr.ids.offset, SEEK_SET) != 0) {
    PLOG(ERROR) << "fseek() failed";
    return false;
  }
  ids->resize(id_count);
  if (!Read(ids->data(), attr.ids.size)) {
    return false;
  }
  return true;
}

bool RecordFileReader::ReadDataSection(
    const std::function<bool(std::unique_ptr<Record>)>& callback, bool sorted) {
  if (fseek(record_fp_, header_.data.offset, SEEK_SET) != 0) {
    PLOG(ERROR) << "fseek() failed";
    return false;
  }
  bool has_timestamp = true;
  for (const auto& attr : file_attrs_) {
    if (!IsTimestampSupported(attr.attr)) {
      has_timestamp = false;
      break;
    }
  }
  RecordCache cache(has_timestamp);
  for (size_t nbytes_read = 0; nbytes_read < header_.data.size;) {
    std::unique_ptr<Record> record = ReadRecord(&nbytes_read);
    if (record == nullptr) {
      return false;
    }
    if (record->type() == SIMPLE_PERF_RECORD_EVENT_ID) {
      ProcessEventIdRecord(*static_cast<EventIdRecord*>(record.get()));
    }
    if (sorted) {
      cache.Push(std::move(record));
      record = cache.Pop();
      if (record != nullptr) {
        if (!callback(std::move(record))) {
          return false;
        }
      }
    } else {
      if (!callback(std::move(record))) {
        return false;
      }
    }
  }
  std::vector<std::unique_ptr<Record>> records = cache.PopAll();
  for (auto& record : records) {
    if (!callback(std::move(record))) {
      return false;
    }
  }
  return true;
}

std::unique_ptr<Record> RecordFileReader::ReadRecord(size_t* nbytes_read) {
  char header_buf[Record::header_size()];
  if (!Read(header_buf, Record::header_size())) {
    return nullptr;
  }
  RecordHeader header(header_buf);
  std::unique_ptr<char[]> p;
  if (header.type == SIMPLE_PERF_RECORD_SPLIT) {
    // Read until meeting a RECORD_SPLIT_END record.
    std::vector<char> buf;
    size_t cur_size = 0;
    char header_buf[Record::header_size()];
    while (header.type == SIMPLE_PERF_RECORD_SPLIT) {
      size_t bytes_to_read = header.size - Record::header_size();
      buf.resize(cur_size + bytes_to_read);
      if (!Read(&buf[cur_size], bytes_to_read)) {
        return nullptr;
      }
      cur_size += bytes_to_read;
      *nbytes_read += header.size;
      if (!Read(header_buf, Record::header_size())) {
        return nullptr;
      }
      header = RecordHeader(header_buf);
    }
    if (header.type != SIMPLE_PERF_RECORD_SPLIT_END) {
      LOG(ERROR) << "SPLIT records are not followed by a SPLIT_END record.";
      return nullptr;
    }
    *nbytes_read += header.size;
    header = RecordHeader(buf.data());
    p.reset(new char[header.size]);
    memcpy(p.get(), buf.data(), buf.size());
  } else {
    p.reset(new char[header.size]);
    memcpy(p.get(), header_buf, Record::header_size());
    if (header.size > Record::header_size()) {
      if (!Read(p.get() + Record::header_size(), header.size - Record::header_size())) {
        return nullptr;
      }
    }
    *nbytes_read += header.size;
  }

  const perf_event_attr* attr = &file_attrs_[0].attr;
  if (file_attrs_.size() > 1 && header.type < PERF_RECORD_USER_DEFINED_TYPE_START) {
    bool has_event_id = false;
    uint64_t event_id;
    if (header.type == PERF_RECORD_SAMPLE) {
      if (header.size > event_id_pos_in_sample_records_ + sizeof(uint64_t)) {
        has_event_id = true;
        event_id = *reinterpret_cast<uint64_t*>(p.get() + event_id_pos_in_sample_records_);
      }
    } else {
      if (header.size > event_id_reverse_pos_in_non_sample_records_) {
        has_event_id = true;
        event_id = *reinterpret_cast<uint64_t*>(p.get() + header.size - event_id_reverse_pos_in_non_sample_records_);
      }
    }
    if (has_event_id) {
      auto it = event_id_to_attr_map_.find(event_id);
      if (it != event_id_to_attr_map_.end()) {
        attr = it->second;
      }
    }
  }
  return ReadRecordFromOwnedBuffer(*attr, header.type, p.release());
}

bool RecordFileReader::Read(void* buf, size_t len) {
  if (len != 0 && fread(buf, len, 1, record_fp_) != 1) {
    PLOG(FATAL) << "failed to read file " << filename_;
    return false;
  }
  return true;
}

void RecordFileReader::ProcessEventIdRecord(const EventIdRecord& r) {
  for (size_t i = 0; i < r.count; ++i) {
    event_ids_for_file_attrs_[r.data[i].attr_id].push_back(r.data[i].event_id);
    event_id_to_attr_map_[r.data[i].event_id] =
        &file_attrs_[r.data[i].attr_id].attr;
  }
}

bool RecordFileReader::ReadFeatureSection(int feature, std::vector<char>* data) {
  const std::map<int, SectionDesc>& section_map = FeatureSectionDescriptors();
  auto it = section_map.find(feature);
  if (it == section_map.end()) {
    return false;
  }
  SectionDesc section = it->second;
  data->resize(section.size);
  if (section.size == 0) {
    return true;
  }
  if (fseek(record_fp_, section.offset, SEEK_SET) != 0) {
    PLOG(ERROR) << "fseek() failed";
    return false;
  }
  if (!Read(data->data(), data->size())) {
    return false;
  }
  return true;
}

std::vector<std::string> RecordFileReader::ReadCmdlineFeature() {
  std::vector<char> buf;
  if (!ReadFeatureSection(FEAT_CMDLINE, &buf)) {
    return std::vector<std::string>();
  }
  const char* p = buf.data();
  const char* end = buf.data() + buf.size();
  std::vector<std::string> cmdline;
  uint32_t arg_count;
  MoveFromBinaryFormat(arg_count, p);
  CHECK_LE(p, end);
  for (size_t i = 0; i < arg_count; ++i) {
    uint32_t len;
    MoveFromBinaryFormat(len, p);
    CHECK_LE(p + len, end);
    cmdline.push_back(p);
    p += len;
  }
  return cmdline;
}

std::vector<BuildIdRecord> RecordFileReader::ReadBuildIdFeature() {
  std::vector<char> buf;
  if (!ReadFeatureSection(FEAT_BUILD_ID, &buf)) {
    return std::vector<BuildIdRecord>();
  }
  const char* p = buf.data();
  const char* end = buf.data() + buf.size();
  std::vector<BuildIdRecord> result;
  while (p < end) {
    BuildIdRecord record(p);
    // Set type explicitly as the perf.data produced by perf doesn't set it.
    record.SetTypeAndMisc(PERF_RECORD_BUILD_ID, record.misc());
    CHECK_LE(p + record.size(), end);
    p += record.size();
    result.push_back(std::move(record));
  }
  return result;
}

std::string RecordFileReader::ReadFeatureString(int feature) {
  std::vector<char> buf;
  if (!ReadFeatureSection(feature, &buf)) {
    return std::string();
  }
  const char* p = buf.data();
  const char* end = buf.data() + buf.size();
  uint32_t len;
  MoveFromBinaryFormat(len, p);
  CHECK_LE(p + len, end);
  return p;
}

std::vector<std::unique_ptr<Record>> RecordFileReader::DataSection() {
  std::vector<std::unique_ptr<Record>> records;
  ReadDataSection([&](std::unique_ptr<Record> record) {
    records.push_back(std::move(record));
    return true;
  });
  return records;
}
