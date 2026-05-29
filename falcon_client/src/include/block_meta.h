/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

struct FalconBlockStatResult {
    std::string key;
    uint64_t size = 0;
    uint64_t offset = 0;
    std::string filePath;
    int64_t atime = 0;
    int64_t mtime = 0;
    int64_t ctime = 0;
    uint64_t version = 0;
    uint32_t state = 0;
};

int FalconBlockPut(const std::string &key, const char *buffer, size_t size);
int FalconBlockGet(const std::string &key, char *buffer, size_t bufferSize);
int FalconBlockDel(const std::string &key);
int FalconBlockStat(const std::string &key, FalconBlockStatResult *result);
