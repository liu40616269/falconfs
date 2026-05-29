/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

#ifndef FALCON_SIZE_FILE_TABLE_H
#define FALCON_SIZE_FILE_TABLE_H

#include "metadb/metadata.h"

#define Natts_falcon_size_file_table 7
#define Anum_falcon_size_file_table_size 1
#define Anum_falcon_size_file_table_file_path 2
#define Anum_falcon_size_file_table_next_offset 3
#define Anum_falcon_size_file_table_capacity 4
#define Anum_falcon_size_file_table_state 5
#define Anum_falcon_size_file_table_create_time 6
#define Anum_falcon_size_file_table_update_time 7

typedef enum FalconSizeFileTableScankeyType {
    SIZE_FILE_TABLE_SIZE_EQ,
    LAST_FALCON_SIZE_FILE_TABLE_SCANKEY_TYPE
} FalconSizeFileTableScankeyType;

extern const char *SizeFileTableName;

void ConstructCreateSizeFileTableCommand(StringInfo command, const char *name);

#endif
