/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

#pragma once

#include <cstdint>
#include <string>

class SizeFileStore {
  public:
    static int CreateSizeFile(const std::string &filePath, uint64_t capacity);
    static int Write(const std::string &filePath, uint64_t offset, const char *buffer, uint64_t size);
    static int Read(const std::string &filePath, uint64_t offset, char *buffer, uint64_t size);
};
