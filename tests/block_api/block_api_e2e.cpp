/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

#include "block_meta.h"
#include "falcon_meta.h"
#include "remote_connection_utils/error_code_def.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
int Check(bool condition, const std::string &message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        return 1;
    }
    return 0;
}

int CheckCode(int actual, int expected, const std::string &operation)
{
    if (actual != expected) {
        std::cerr << "FAIL: " << operation << " returned " << actual << ", expected " << expected << std::endl;
        return 1;
    }
    return 0;
}
} // namespace

int main(int argc, char **argv)
{
    std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    int port = argc > 2 ? std::stoi(argv[2]) : 55500;

    router = std::make_shared<Router>(ServerIdentifier(host, port));

    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string key = "block_e2e_" + std::to_string(now);
    const std::string data = "falcon-block-e2e-data-0001";
    const std::string updated = "falcon-block-e2e-data-0002";
    std::vector<char> buffer(data.size(), '\0');

    int ret = FalconBlockPut(key, data.data(), data.size());
    if (CheckCode(ret, SUCCESS, "FalconBlockPut(new key)") != 0) {
        return 1;
    }

    ret = FalconBlockGet(key, buffer.data(), buffer.size());
    if (CheckCode(ret, SUCCESS, "FalconBlockGet(new key)") != 0) {
        return 1;
    }
    if (Check(std::memcmp(buffer.data(), data.data(), data.size()) == 0, "read data mismatch") != 0) {
        return 1;
    }

    FalconBlockStatResult stat;
    ret = FalconBlockStat(key, &stat);
    if (CheckCode(ret, SUCCESS, "FalconBlockStat") != 0) {
        return 1;
    }
    if (Check(stat.key == key, "stat key mismatch") != 0 ||
        Check(stat.size == data.size(), "stat size mismatch") != 0 ||
        Check(!stat.filePath.empty(), "stat filePath empty") != 0) {
        return 1;
    }

    ret = FalconBlockPut(key, updated.data(), updated.size());
    if (CheckCode(ret, SUCCESS, "FalconBlockPut(overwrite)") != 0) {
        return 1;
    }

    std::fill(buffer.begin(), buffer.end(), '\0');
    ret = FalconBlockGet(key, buffer.data(), buffer.size());
    if (CheckCode(ret, SUCCESS, "FalconBlockGet(overwrite)") != 0) {
        return 1;
    }
    if (Check(std::memcmp(buffer.data(), updated.data(), updated.size()) == 0, "overwrite data mismatch") != 0) {
        return 1;
    }

    ret = FalconBlockDel(key);
    if (CheckCode(ret, SUCCESS, "FalconBlockDel") != 0) {
        return 1;
    }

    ret = FalconBlockGet(key, buffer.data(), buffer.size());
    if (CheckCode(ret, FILE_NOT_EXISTS, "FalconBlockGet(after delete)") != 0) {
        return 1;
    }

    std::cout << "PASS BlockApiE2E key=" << key << " size=" << data.size() << " file=" << stat.filePath
              << " offset=" << stat.offset << std::endl;
    return 0;
}
