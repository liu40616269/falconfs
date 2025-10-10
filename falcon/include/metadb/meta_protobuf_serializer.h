/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

#ifndef META_PROTOBUF_SERIALIZER_H
#define META_PROTOBUF_SERIALIZER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "metadb/meta_process_info.h"

/*
 * Protobuf serialization functions for response data.
 * These functions serialize response data using protobuf format.
 * The caller is responsible for freeing the returned buffer.
 */

/*
 * Serialize a simple response (error code only) using protobuf.
 * Returns serialized data size, or 0 on failure.
 * The buffer is allocated and must be freed by caller using ProtobufSerializerFreeBuffer.
 */
size_t ProtobufSerializeSimpleResponse(int32_t errorCode, char **outBuffer);

/*
 * Serialize CREATE response using protobuf.
 */
size_t ProtobufSerializeCreateResponse(MetaProcessInfoData *info, char **outBuffer);

/*
 * Serialize STAT response using protobuf.
 */
size_t ProtobufSerializeStatResponse(MetaProcessInfoData *info, char **outBuffer);

/*
 * Serialize OPEN response using protobuf.
 */
size_t ProtobufSerializeOpenResponse(MetaProcessInfoData *info, char **outBuffer);

/*
 * Serialize UNLINK response using protobuf.
 */
size_t ProtobufSerializeUnlinkResponse(MetaProcessInfoData *info, char **outBuffer);

/*
 * Serialize OPENDIR response using protobuf.
 */
size_t ProtobufSerializeOpenDirResponse(MetaProcessInfoData *info, char **outBuffer);

/*
 * Serialize READDIR response using protobuf.
 */
size_t ProtobufSerializeReadDirResponse(MetaProcessInfoData *info, char **outBuffer);

/*
 * Serialize KV_GET response using protobuf.
 */
size_t ProtobufSerializeKvGetResponse(KvMetaProcessInfoData *info, char **outBuffer);

/*
 * Serialize SLICE_GET response using protobuf.
 */
size_t ProtobufSerializeSliceGetResponse(SliceProcessInfoData *info, char **outBuffer);

/*
 * Serialize FETCH_SLICE_ID response using protobuf.
 */
size_t ProtobufSerializeFetchSliceIdResponse(SliceIdProcessInfoData *info, char **outBuffer);

/*
 * Free buffer allocated by protobuf serialization functions.
 */
void ProtobufSerializerFreeBuffer(char *buffer);

/*
 * Request parsing functions - parse protobuf request data into MetaProcessInfoData
 */

/*
 * Parse PathOnly request (MKDIR, CREATE, STAT, OPEN, UNLINK, OPENDIR, RMDIR)
 * Returns allocated path string, caller must pfree it.
 */
char* ProtobufParsePathOnlyRequest(const char *data, size_t size);

/*
 * Parse CLOSE request
 */
int ProtobufParseCloseRequest(const char *data, size_t size, MetaProcessInfoData *info);

/*
 * Parse READDIR request
 */
int ProtobufParseReadDirRequest(const char *data, size_t size, MetaProcessInfoData *info);

/*
 * Parse MKDIR_SUB_MKDIR request
 */
int ProtobufParseMkdirSubMkdirRequest(const char *data, size_t size, MetaProcessInfoData *info);

/*
 * Parse MKDIR_SUB_CREATE request
 */
int ProtobufParseMkdirSubCreateRequest(const char *data, size_t size, MetaProcessInfoData *info);

/*
 * Parse RMDIR_SUB_RMDIR request
 */
int ProtobufParseRmdirSubRmdirRequest(const char *data, size_t size, MetaProcessInfoData *info);

/*
 * Parse RMDIR_SUB_UNLINK request
 */
int ProtobufParseRmdirSubUnlinkRequest(const char *data, size_t size, MetaProcessInfoData *info);

/*
 * Parse RENAME request
 */
int ProtobufParseRenameRequest(const char *data, size_t size, MetaProcessInfoData *info);

/*
 * Parse RENAME_SUB_RENAME_LOCALLY request
 */
int ProtobufParseRenameSubRenameLocallyRequest(const char *data, size_t size, MetaProcessInfoData *info);

/*
 * Parse RENAME_SUB_CREATE request
 */
int ProtobufParseRenameSubCreateRequest(const char *data, size_t size, MetaProcessInfoData *info);

/*
 * Parse UTIMENS request
 */
int ProtobufParseUtimeNsRequest(const char *data, size_t size, MetaProcessInfoData *info);

/*
 * Parse CHOWN request
 */
int ProtobufParseChownRequest(const char *data, size_t size, MetaProcessInfoData *info);

/*
 * Parse CHMOD request
 */
int ProtobufParseChmodRequest(const char *data, size_t size, MetaProcessInfoData *info);

/*
 * Parse KV_PUT request
 */
int ProtobufParseKvPutRequest(const char *data, size_t size, KvMetaProcessInfoData *info);

/*
 * Parse KV_GET/KV_DEL request
 */
int ProtobufParseKvKeyOnlyRequest(const char *data, size_t size, KvMetaProcessInfoData *info);

/*
 * Parse PLAIN_COMMAND request
 */
char* ProtobufParsePlainCommandRequest(const char *data, size_t size);

/*
 * Parse SLICE_GET/SLICE_DEL request
 */
int ProtobufParseSliceIndexRequest(const char *data, size_t size, SliceProcessInfoData *info);

/*
 * Parse SLICE_PUT request
 */
int ProtobufParseSlicePutRequest(const char *data, size_t size, SliceProcessInfoData *info);

/*
 * Parse FETCH_SLICE_ID request
 */
int ProtobufParseFetchSliceIdRequest(const char *data, size_t size, SliceIdProcessInfoData *info);

#ifdef __cplusplus
}
#endif

#endif /* META_PROTOBUF_SERIALIZER_H */
