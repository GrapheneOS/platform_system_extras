/*
**
** Copyright 2016, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include "read_apk.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <ziparchive/zip_archive.h>
#include "read_elf.h"
#include "utils.h"

std::unordered_map<std::string, ApkInspector::ApkNode> ApkInspector::embedded_elf_cache_;

EmbeddedElf* ApkInspector::FindElfInApkByOffset(const std::string& apk_path, uint64_t file_offset) {
  // Already in cache?
  ApkNode& node = embedded_elf_cache_[apk_path];
  auto it = node.offset_map.find(file_offset);
  if (it != node.offset_map.end()) {
    return it->second.get();
  }
  std::unique_ptr<EmbeddedElf> elf = FindElfInApkByOffsetWithoutCache(apk_path, file_offset);
  EmbeddedElf* result = elf.get();
  node.offset_map[file_offset] = std::move(elf);
  if (result != nullptr) {
    node.name_map[result->entry_name()] = result;
  }
  return result;
}

EmbeddedElf* ApkInspector::FindElfInApkByName(const std::string& apk_path,
                                              const std::string& entry_name) {
  ApkNode& node = embedded_elf_cache_[apk_path];
  auto it = node.name_map.find(entry_name);
  if (it != node.name_map.end()) {
    return it->second;
  }
  std::unique_ptr<EmbeddedElf> elf = FindElfInApkByNameWithoutCache(apk_path, entry_name);
  EmbeddedElf* result = elf.get();
  node.name_map[entry_name] = result;
  if (result != nullptr) {
    node.offset_map[result->entry_offset()] = std::move(elf);
  }
  return result;
}

std::unique_ptr<EmbeddedElf> ApkInspector::FindElfInApkByOffsetWithoutCache(
    const std::string& apk_path, uint64_t file_offset) {
  // Crack open the apk(zip) file and take a look.
  if (!IsValidApkPath(apk_path)) {
    return nullptr;
  }

  FileHelper fhelper = FileHelper::OpenReadOnly(apk_path);
  if (!fhelper) {
    return nullptr;
  }

  ArchiveHelper ahelper(fhelper.fd(), apk_path);
  if (!ahelper) {
    return nullptr;
  }
  ZipArchiveHandle &handle = ahelper.archive_handle();

  // Iterate through the zip file. Look for a zip entry corresponding
  // to an uncompressed blob whose range intersects with the mmap
  // offset we're interested in.
  void* iteration_cookie;
  if (StartIteration(handle, &iteration_cookie, nullptr, nullptr) < 0) {
    return nullptr;
  }
  ZipEntry zentry;
  ZipString zname;
  bool found = false;
  int zrc;
  while ((zrc = Next(iteration_cookie, &zentry, &zname)) == 0) {
    if (zentry.method == kCompressStored &&
        file_offset >= static_cast<uint64_t>(zentry.offset) &&
        file_offset < static_cast<uint64_t>(zentry.offset + zentry.uncompressed_length)) {
      // Found.
      found = true;
      break;
    }
  }
  EndIteration(iteration_cookie);
  if (!found) {
    return nullptr;
  }

  // We found something in the zip file at the right spot. Is it an ELF?
  if (lseek(fhelper.fd(), zentry.offset, SEEK_SET) != zentry.offset) {
    PLOG(ERROR) << "lseek() failed in " << apk_path << " offset " << zentry.offset;
    return nullptr;
  }
  std::string entry_name;
  entry_name.resize(zname.name_length,'\0');
  memcpy(&entry_name[0], zname.name, zname.name_length);
  ElfStatus result = IsValidElfFile(fhelper.fd());
  if (result != ElfStatus::NO_ERROR) {
    // Omit files that are not ELF files.
    return nullptr;
  }
  return std::unique_ptr<EmbeddedElf>(new EmbeddedElf(apk_path, entry_name, zentry.offset,
                                                      zentry.uncompressed_length));
}

std::unique_ptr<EmbeddedElf> ApkInspector::FindElfInApkByNameWithoutCache(
    const std::string& apk_path, const std::string& entry_name) {
  if (!IsValidApkPath(apk_path)) {
    return nullptr;
  }
  FileHelper fhelper = FileHelper::OpenReadOnly(apk_path);
  if (!fhelper) {
    return nullptr;
  }
  ArchiveHelper ahelper(fhelper.fd(), apk_path);
  if (!ahelper) {
    return nullptr;
  }
  ZipArchiveHandle& handle = ahelper.archive_handle();
  ZipEntry zentry;
  int32_t rc = FindEntry(handle, ZipString(entry_name.c_str()), &zentry);
  if (rc != 0) {
    LOG(ERROR) << "failed to find " << entry_name << " in " << apk_path
        << ": " << ErrorCodeString(rc);
    return nullptr;
  }
  if (zentry.method != kCompressStored || zentry.compressed_length != zentry.uncompressed_length) {
    LOG(ERROR) << "shared library " << entry_name << " in " << apk_path << " is compressed";
    return nullptr;
  }
  return std::unique_ptr<EmbeddedElf>(new EmbeddedElf(apk_path, entry_name, zentry.offset,
                                                      zentry.uncompressed_length));
}

bool IsValidApkPath(const std::string& apk_path) {
  static const char zip_preamble[] = {0x50, 0x4b, 0x03, 0x04 };
  if (!IsRegularFile(apk_path)) {
    return false;
  }
  std::string mode = std::string("rb") + CLOSE_ON_EXEC_MODE;
  FILE* fp = fopen(apk_path.c_str(), mode.c_str());
  if (fp == nullptr) {
    return false;
  }
  char buf[4];
  if (fread(buf, 4, 1, fp) != 1) {
    fclose(fp);
    return false;
  }
  fclose(fp);
  return memcmp(buf, zip_preamble, 4) == 0;
}

// Refer file in apk in compliance with http://developer.android.com/reference/java/net/JarURLConnection.html.
std::string GetUrlInApk(const std::string& apk_path, const std::string& elf_filename) {
  return apk_path + "!/" + elf_filename;
}

std::tuple<bool, std::string, std::string> SplitUrlInApk(const std::string& path) {
  size_t pos = path.find("!/");
  if (pos == std::string::npos) {
    return std::make_tuple(false, "", "");
  }
  return std::make_tuple(true, path.substr(0, pos), path.substr(pos + 2));
}
