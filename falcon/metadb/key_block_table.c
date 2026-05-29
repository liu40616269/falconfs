/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

#include "metadb/key_block_table.h"

const char *KeyBlockTableName = "falcon_key_block_table";

void ConstructCreateKeyBlockTableCommand(StringInfo command, const char *name)
{
    appendStringInfo(command,
                     "CREATE TABLE falcon.%s("
                     "key text,"
                     "size bigint,"
                      "\"offset\" bigint,"
                     "atime timestamptz,"
                     "mtime timestamptz,"
                     "ctime timestamptz,"
                     "version bigint,"
                     "state int);"
                     "CREATE UNIQUE INDEX %s_index ON falcon.%s USING btree(key);"
                      "CREATE INDEX %s_size_offset_index ON falcon.%s USING btree(size, \"offset\");"
                     "ALTER TABLE falcon.%s SET SCHEMA pg_catalog;"
                     "GRANT SELECT ON pg_catalog.%s TO public;"
                     "ALTER EXTENSION falcon ADD TABLE %s;",
                     name,
                     name,
                     name,
                     name,
                     name,
                     name,
                     name,
                     name);
}
