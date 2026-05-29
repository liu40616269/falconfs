/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

#include "metadb/size_file_table.h"

const char *SizeFileTableName = "falcon_size_file_table";

void ConstructCreateSizeFileTableCommand(StringInfo command, const char *name)
{
    appendStringInfo(command,
                     "CREATE TABLE falcon.%s("
                     "size bigint,"
                     "file_path text,"
                     "next_offset bigint,"
                     "capacity bigint,"
                     "state int,"
                     "create_time timestamptz,"
                     "update_time timestamptz);"
                     "CREATE UNIQUE INDEX %s_index ON falcon.%s USING btree(size);"
                     "ALTER TABLE falcon.%s SET SCHEMA pg_catalog;"
                     "GRANT SELECT ON pg_catalog.%s TO public;"
                     "ALTER EXTENSION falcon ADD TABLE %s;",
                     name,
                     name,
                     name,
                     name,
                     name,
                     name);
}
