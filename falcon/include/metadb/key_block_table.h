/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

#ifndef FALCON_KEY_BLOCK_TABLE_H
#define FALCON_KEY_BLOCK_TABLE_H

#include "metadb/metadata.h"

#define Natts_falcon_key_block_table 8
#define Anum_falcon_key_block_table_key 1
#define Anum_falcon_key_block_table_size 2
#define Anum_falcon_key_block_table_offset 3
#define Anum_falcon_key_block_table_atime 4
#define Anum_falcon_key_block_table_mtime 5
#define Anum_falcon_key_block_table_ctime 6
#define Anum_falcon_key_block_table_version 7
#define Anum_falcon_key_block_table_state 8

typedef enum FalconKeyBlockTableScankeyType {
    KEY_BLOCK_TABLE_KEY_EQ,
    KEY_BLOCK_TABLE_SIZE_EQ,
    KEY_BLOCK_TABLE_OFFSET_EQ,
    LAST_FALCON_KEY_BLOCK_TABLE_SCANKEY_TYPE
} FalconKeyBlockTableScankeyType;

extern const char *KeyBlockTableName;

void ConstructCreateKeyBlockTableCommand(StringInfo command, const char *name);

#endif
