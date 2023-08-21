#pragma once

#include "meminspect.h"

/**
 * @brief Generate a pinlist file from a given list of vmas containing a list of 4-byte pairs
 * representing (4-byte offset, 4-byte len) contiguous in memory and they are stored in big endian
 * format.
 *
 * @param output_file Output file to write pinlist
 * @param vmas_to_pin Set of vmas to write into pinlist file.
 * @return 0 on success, non-zero on failure
 */
int write_pinlist_file(const std::string& output_file, const std::vector<VmaRange>& vmas_to_pin);

/**
 * @brief This method is the counter part of @see write_pinlist_file(). It will read an existing
 * pinlist file.
 *
 * @param pinner_file Input pinlist file
 * @param pinranges Vmas read from pinlist file. This is populated on call.
 * @return 0 on success, non-zero on failure
 */
int read_pinlist_file(const std::string& pinner_file, /*out*/ std::vector<VmaRange>& pinranges);