/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

#include "postgres.h"

#include "fmgr.h"
#include "executor/spi.h"
#include "utils/palloc.h"

#include <unistd.h>
#include <string.h>

#include "connection_pool/connection_pool.h"
#include "metadb/meta_handle.h"
#include "metadb/meta_process_info.h"
#include "metadb/meta_protobuf_serializer.h"
#include "utils/error_log.h"

PG_FUNCTION_INFO_V1(falcon_kv_meta_call_by_shmem_internal);

static inline const char* MetaServiceTypeNameFromProto(int32_t type)
{
    switch (type) {
        case 0: return "PLAIN_COMMAND";
        case 1: return "MKDIR";
        case 2: return "MKDIR_SUB_MKDIR";
        case 3: return "MKDIR_SUB_CREATE";
        case 4: return "CREATE";
        case 5: return "STAT";
        case 6: return "OPEN";
        case 7: return "CLOSE";
        case 8: return "UNLINK";
        case 9: return "READDIR";
        case 10: return "OPENDIR";
        case 11: return "RMDIR";
        case 12: return "RMDIR_SUB_RMDIR";
        case 13: return "RMDIR_SUB_UNLINK";
        case 14: return "RENAME";
        case 15: return "RENAME_SUB_RENAME_LOCALLY";
        case 16: return "RENAME_SUB_CREATE";
        case 17: return "UTIMENS";
        case 18: return "CHOWN";
        case 19: return "CHMOD";
        case 20: return "KV_PUT";
        case 21: return "KV_GET";
        case 22: return "KV_DEL";
        case 23: return "SLICE_PUT";
        case 24: return "SLICE_GET";
        case 25: return "SLICE_DEL";
        case 26: return "FETCH_SLICE_ID";
        default: return "UNKNOWN";
    }
}

static Datum falcon_batch_slice_call(int32_t operation_type, uint32_t count, char *paramBuffer, int64_t signature);
static Datum falcon_batch_sliceid_call(char *paramBuffer, int64_t signature);

Datum falcon_kv_meta_call_by_shmem_internal(PG_FUNCTION_ARGS)
{
    int32_t operation_type = PG_GETARG_INT32(0);
    uint32_t count = PG_GETARG_INT32(1);
    uint64_t paramShmemShift = (uint64_t)PG_GETARG_INT64(2);
    int64_t signature = PG_GETARG_INT64(3);

    printf("[debug] falcon_kv_meta_call_by_shmem_internal: ENTRY, operation_type=%d(%s), count=%u\n", operation_type, MetaServiceTypeNameFromProto(operation_type), count);
    fflush(stdout);

    /* 1. 从共享内存获取数据 */
    FalconShmemAllocator *allocator = &FalconConnectionPoolShmemAllocator;
    if (paramShmemShift > allocator->pageCount * FALCON_SHMEM_ALLOCATOR_PAGE_SIZE)
        FALCON_ELOG_ERROR(ARGUMENT_ERROR, "paramShmemShift is invalid.");

    char *paramBuffer = FALCON_SHMEM_ALLOCATOR_GET_POINTER(allocator, paramShmemShift);

    if (count == 0 || count > 1000) {  /* 安全检查 */
        FALCON_ELOG_ERROR(ARGUMENT_ERROR, "Invalid count");
    }

    if (count != 1 && !(operation_type == 1 ||   /* MKDIR */
                        operation_type == 4 ||   /* CREATE */
                        operation_type == 5 ||   /* STAT */
                        operation_type == 6 ||   /* OPEN */
                        operation_type == 7 ||   /* CLOSE */
                        operation_type == 8)) {  /* UNLINK */
        FALCON_ELOG_ERROR_EXTENDED(ARGUMENT_ERROR,
            "Operation type %d doesn't support batch operation, but count=%u",
            operation_type, count);
    }

    /* 特殊处理 FETCH_SLICE_ID 操作 (26=FETCH_SLICE_ID, protobuf枚举值) */
    if (operation_type == 26) {
        return falcon_batch_sliceid_call(paramBuffer, signature);
    }

    /* 特殊处理 SLICE 操作 (23=SLICE_PUT, 24=SLICE_GET, 25=SLICE_DEL, protobuf枚举值) */
    if (operation_type >= 23 && operation_type <= 25) {
        return falcon_batch_slice_call(operation_type, count, paramBuffer, signature);
    }

    char *p = paramBuffer;

    /* 3. 构造 MetaProcessInfo 数组或 KvMetaProcessInfo 数组 */
    void *data = NULL;
    MetaProcessInfoData *infoDataArray = NULL;
    MetaProcessInfo *infoArray = NULL;
    KvMetaProcessInfoData *kvInfoArray = NULL;

    /* 对于 KV 操作，分配 KvMetaProcessInfo 数组；对于其他操作，分配 MetaProcessInfo 数组 */
    if (operation_type >= 20 && operation_type <= 22) {
        kvInfoArray = palloc(sizeof(KvMetaProcessInfoData) * count);
        memset(kvInfoArray, 0, sizeof(KvMetaProcessInfoData) * count);
    } else {
        data = palloc((sizeof(MetaProcessInfoData) + sizeof(MetaProcessInfo)) * count);
        infoDataArray = data;
        infoArray = (MetaProcessInfo *)(infoDataArray + count);
    }

    /* 使用 protobuf 解析请求 */
    for (uint32_t i = 0; i < count; i++) {
        /* 读取 protobuf 长度前缀 */
        uint32_t pbLen = *(uint32_t*)p;
        p += sizeof(uint32_t);
        char *pbData = p;
        p += pbLen;

        /* 根据操作类型解析 protobuf 参数 */
        switch (operation_type) {
            case 20:  /* KV_PUT */
                ProtobufParseKvPutRequest(pbData, pbLen, &kvInfoArray[i]);
                break;

            case 21:  /* KV_GET */
            case 22:  /* KV_DEL */
                ProtobufParseKvKeyOnlyRequest(pbData, pbLen, &kvInfoArray[i]);
                break;

            case 1:  /* MKDIR */
            case 4:  /* CREATE */
            case 5:  /* STAT */
            case 6:  /* OPEN */
            case 8:  /* UNLINK */
            case 10: /* OPENDIR */
            case 11: /* RMDIR */
                infoArray[i] = &infoDataArray[i];
                memset(&infoDataArray[i], 0, sizeof(MetaProcessInfoData));
                infoDataArray[i].path = ProtobufParsePathOnlyRequest(pbData, pbLen);
                break;

            case 9:  /* READDIR */
                infoArray[i] = &infoDataArray[i];
                memset(&infoDataArray[i], 0, sizeof(MetaProcessInfoData));
                ProtobufParseReadDirRequest(pbData, pbLen, &infoDataArray[i]);
                break;

            case 2: /* MKDIR_SUB_MKDIR */
                infoArray[i] = &infoDataArray[i];
                memset(&infoDataArray[i], 0, sizeof(MetaProcessInfoData));
                ProtobufParseMkdirSubMkdirRequest(pbData, pbLen, &infoDataArray[i]);
                break;

            case 3: /* MKDIR_SUB_CREATE */
                infoArray[i] = &infoDataArray[i];
                memset(&infoDataArray[i], 0, sizeof(MetaProcessInfoData));
                ProtobufParseMkdirSubCreateRequest(pbData, pbLen, &infoDataArray[i]);
                break;

            case 12: /* RMDIR_SUB_RMDIR */
                infoArray[i] = &infoDataArray[i];
                memset(&infoDataArray[i], 0, sizeof(MetaProcessInfoData));
                ProtobufParseRmdirSubRmdirRequest(pbData, pbLen, &infoDataArray[i]);
                break;

            case 13: /* RMDIR_SUB_UNLINK */
                infoArray[i] = &infoDataArray[i];
                memset(&infoDataArray[i], 0, sizeof(MetaProcessInfoData));
                ProtobufParseRmdirSubUnlinkRequest(pbData, pbLen, &infoDataArray[i]);
                break;

            case 14: /* RENAME */
                infoArray[i] = &infoDataArray[i];
                memset(&infoDataArray[i], 0, sizeof(MetaProcessInfoData));
                ProtobufParseRenameRequest(pbData, pbLen, &infoDataArray[i]);
                break;

            case 15: /* RENAME_SUB_RENAME_LOCALLY */
                infoArray[i] = &infoDataArray[i];
                memset(&infoDataArray[i], 0, sizeof(MetaProcessInfoData));
                ProtobufParseRenameSubRenameLocallyRequest(pbData, pbLen, &infoDataArray[i]);
                break;

            case 16: /* RENAME_SUB_CREATE */
                infoArray[i] = &infoDataArray[i];
                memset(&infoDataArray[i], 0, sizeof(MetaProcessInfoData));
                ProtobufParseRenameSubCreateRequest(pbData, pbLen, &infoDataArray[i]);
                break;

            case 7:  /* CLOSE */
                infoArray[i] = &infoDataArray[i];
                memset(&infoDataArray[i], 0, sizeof(MetaProcessInfoData));
                ProtobufParseCloseRequest(pbData, pbLen, &infoDataArray[i]);
                break;

            case 17: /* UTIMENS */
                infoArray[i] = &infoDataArray[i];
                memset(&infoDataArray[i], 0, sizeof(MetaProcessInfoData));
                ProtobufParseUtimeNsRequest(pbData, pbLen, &infoDataArray[i]);
                break;

            case 18: /* CHOWN */
                infoArray[i] = &infoDataArray[i];
                memset(&infoDataArray[i], 0, sizeof(MetaProcessInfoData));
                ProtobufParseChownRequest(pbData, pbLen, &infoDataArray[i]);
                break;

            case 19: /* CHMOD */
                infoArray[i] = &infoDataArray[i];
                memset(&infoDataArray[i], 0, sizeof(MetaProcessInfoData));
                ProtobufParseChmodRequest(pbData, pbLen, &infoDataArray[i]);
                break;

            default:
                FALCON_ELOG_ERROR(ARGUMENT_ERROR, "Unsupported operation type");
        }
    }

    switch (operation_type) {
        case 20:  /* KV_PUT (protobuf enum) */
            for (uint32_t i = 0; i < count; i++) {
                FalconKvmetaPutHandle(&kvInfoArray[i]);
            }
            break;
        case 21:  /* KV_GET (protobuf enum) */
            for (uint32_t i = 0; i < count; i++) {
                FalconKvmetaGetHandle(&kvInfoArray[i]);
            }
            break;
        case 22:  /* KV_DEL (protobuf enum) */
            for (uint32_t i = 0; i < count; i++) {
                FalconKvmetaDelHandle(&kvInfoArray[i]);
            }
            break;
        case 1:  /* MKDIR (protobuf enum) */
            FalconMkdirHandle(infoArray, count);
            break;
        case 4:  /* CREATE (protobuf enum) */
            FalconCreateHandle(infoArray, count, false);
            break;
        case 5:  /* STAT (protobuf enum) */
            FalconStatHandle(infoArray, count);
            break;
        case 6:  /* OPEN (protobuf enum) */
            FalconOpenHandle(infoArray, count);
            break;
        case 7:  /* CLOSE (protobuf enum) */
            FalconCloseHandle(infoArray, count);
            break;
        case 8:  /* UNLINK (protobuf enum) */
            FalconUnlinkHandle(infoArray, count);
            break;
        case 9:  /* READDIR (protobuf enum) */
            FalconReadDirHandle(infoArray[0]);  /* READDIR 不支持批处理 */
            break;
        case 10: /* OPENDIR (protobuf enum) */
            FalconOpenDirHandle(infoArray[0]);  /* OPENDIR 不支持批处理 */
            break;
        case 11: /* RMDIR (protobuf enum) */
            FalconRmdirHandle(infoArray[0]);  /* RMDIR 不支持批处理 */
            break;
        case 14: /* RENAME (protobuf enum) */
            FalconRenameHandle(infoArray[0]);  /* RENAME 不支持批处理 */
            break;
        case 2:  /* MKDIR_SUB_MKDIR (protobuf enum) */
            FalconMkdirSubMkdirHandle(infoArray, count);
            break;
        case 3:  /* MKDIR_SUB_CREATE (protobuf enum) */
            FalconMkdirSubCreateHandle(infoArray, count);
            break;
        case 12: /* RMDIR_SUB_RMDIR (protobuf enum) */
            FalconRmdirSubRmdirHandle(infoArray[0]);  /* 不支持批处理 */
            break;
        case 13: /* RMDIR_SUB_UNLINK (protobuf enum) */
            FalconRmdirSubUnlinkHandle(infoArray[0]);  /* 不支持批处理 */
            break;
        case 15: /* RENAME_SUB_RENAME_LOCALLY (protobuf enum) */
            FalconRenameSubRenameLocallyHandle(infoArray[0]);  /* 不支持批处理 */
            break;
        case 16: /* RENAME_SUB_CREATE (protobuf enum) */
            FalconRenameSubCreateHandle(infoArray[0]);  /* 不支持批处理 */
            break;
        case 17: /* UTIMENS (protobuf enum) */
            FalconUtimeNsHandle(infoArray[0]);  /* UTIMENS 不支持批处理 */
            break;
        case 18: /* CHOWN (protobuf enum) */
            FalconChownHandle(infoArray[0]);  /* CHOWN 不支持批处理 */
            break;
        case 19: /* CHMOD (protobuf enum) */
            FalconChmodHandle(infoArray[0]);  /* CHMOD 不支持批处理 */
            break;
        default:
            FALCON_ELOG_ERROR(ARGUMENT_ERROR, "Unsupported operation type");
    }

    /* 5. 使用 protobuf 序列化响应并写入共享内存 */
    /* 先序列化所有响应，计算总大小 */
    char **respBuffers = palloc(sizeof(char*) * count);
    size_t *respSizes = palloc(sizeof(size_t) * count);
    size_t totalSize = 0;

    for (uint32_t i = 0; i < count; i++) {
        char *respBuffer = NULL;
        size_t respSize = 0;

        switch (operation_type) {
            case 20:  /* KV_PUT */
            case 22:  /* KV_DEL */
                respSize = ProtobufSerializeSimpleResponse(kvInfoArray[i].errorCode, &respBuffer);
                break;

            case 21:  /* KV_GET */
                respSize = ProtobufSerializeKvGetResponse(&kvInfoArray[i], &respBuffer);
                break;

            case 1:  /* MKDIR */
            case 7:  /* CLOSE */
            case 11: /* RMDIR */
            case 14: /* RENAME */
            case 12: /* RMDIR_SUB_RMDIR */
            case 13: /* RMDIR_SUB_UNLINK */
            case 15: /* RENAME_SUB_RENAME_LOCALLY */
            case 16: /* RENAME_SUB_CREATE */
            case 17: /* UTIMENS */
            case 18: /* CHOWN */
            case 19: /* CHMOD */
                respSize = ProtobufSerializeSimpleResponse(infoDataArray[i].errorCode, &respBuffer);
                break;

            case 2:  /* MKDIR_SUB_MKDIR */
            case 3:  /* MKDIR_SUB_CREATE */
            case 4:  /* CREATE */
                respSize = ProtobufSerializeCreateResponse(&infoDataArray[i], &respBuffer);
                break;

            case 5:  /* STAT */
                respSize = ProtobufSerializeStatResponse(&infoDataArray[i], &respBuffer);
                break;

            case 6:  /* OPEN */
                respSize = ProtobufSerializeOpenResponse(&infoDataArray[i], &respBuffer);
                break;

            case 8:  /* UNLINK */
                respSize = ProtobufSerializeUnlinkResponse(&infoDataArray[i], &respBuffer);
                break;

            case 9:  /* READDIR */
                respSize = ProtobufSerializeReadDirResponse(&infoDataArray[i], &respBuffer);
                break;

            case 10: /* OPENDIR */
                respSize = ProtobufSerializeOpenDirResponse(&infoDataArray[i], &respBuffer);
                break;

            default:
                FALCON_ELOG_ERROR(ARGUMENT_ERROR, "Unsupported operation type for response");
        }

        respBuffers[i] = respBuffer;
        respSizes[i] = respSize;
        totalSize += sizeof(uint32_t) + respSize;
    }

    /* 分配共享内存 */
    uint64_t responseShmemShift = FalconShmemAllocatorMalloc(allocator, totalSize);
    if (responseShmemShift == 0)
        FALCON_ELOG_ERROR_EXTENDED(PROGRAM_ERROR, "FalconShmemAllocMalloc failed. Size: %lu.", totalSize);

    char *responseBuffer = FALCON_SHMEM_ALLOCATOR_GET_POINTER(allocator, responseShmemShift);
    FALCON_SHMEM_ALLOCATOR_SET_SIGNATURE(responseBuffer, signature);

    /* 写入响应数据 */
    char *resp_p = responseBuffer;
    for (uint32_t i = 0; i < count; i++) {
        /* 写入长度前缀 */
        *(uint32_t*)resp_p = (uint32_t)respSizes[i];
        resp_p += sizeof(uint32_t);

        /* 写入 protobuf 数据 */
        memcpy(resp_p, respBuffers[i], respSizes[i]);
        resp_p += respSizes[i];

        /* 释放临时 buffer */
        ProtobufSerializerFreeBuffer(respBuffers[i]);
    }

    pfree(respBuffers);
    pfree(respSizes);

    printf("[debug] falcon_kv_meta_call_by_shmem_internal: EXIT, returning responseShmemShift=%lu\n", responseShmemShift);
    fflush(stdout);

    /* 6. 返回响应在共享内存中的偏移量 */
    PG_RETURN_INT64(responseShmemShift);
}

/*
 * falcon_batch_sliceid_call
 *
 * FETCH_SLICE_ID 操作的专用处理函数
 */
static Datum falcon_batch_sliceid_call(char *paramBuffer, int64_t signature)
{
    printf("[debug] falcon_batch_sliceid_call: ENTRY\n");
    fflush(stdout);

    char *p = paramBuffer;

    /* 1. 使用 protobuf 解析参数 */
    uint32_t pbLen = *(uint32_t*)p;
    p += sizeof(uint32_t);
    char *pbData = p;

    SliceIdProcessInfoData infoData = {0};
    ProtobufParseFetchSliceIdRequest(pbData, pbLen, &infoData);

    /* 2. 调用核心处理函数 */
    FalconFetchSliceIdHandle(&infoData);

    /* 3. 使用 protobuf 序列化响应 */
    char *respBuffer = NULL;
    size_t respSize = ProtobufSerializeFetchSliceIdResponse(&infoData, &respBuffer);

    /* 4. 分配共享内存 */
    size_t response_size = sizeof(uint32_t) + respSize;
    FalconShmemAllocator *allocator = &FalconConnectionPoolShmemAllocator;
    uint64_t responseShmemShift = FalconShmemAllocatorMalloc(allocator, response_size);
    if (responseShmemShift == 0)
        FALCON_ELOG_ERROR_EXTENDED(PROGRAM_ERROR, "FalconShmemAllocatorMalloc failed. Size: %zu.", response_size);

    char *responseBuffer = FALCON_SHMEM_ALLOCATOR_GET_POINTER(allocator, responseShmemShift);
    FALCON_SHMEM_ALLOCATOR_SET_SIGNATURE(responseBuffer, signature);

    /* 5. 写入响应 */
    char *resp_p = responseBuffer;
    *(uint32_t*)resp_p = (uint32_t)respSize;
    resp_p += sizeof(uint32_t);
    memcpy(resp_p, respBuffer, respSize);

    ProtobufSerializerFreeBuffer(respBuffer);

    printf("[debug] falcon_batch_sliceid_call: EXIT, start=%lu, end=%lu, response_size=%zu\n",
           infoData.start, infoData.end, response_size);
    fflush(stdout);

    PG_RETURN_INT64(responseShmemShift);
}

/*
 * falcon_batch_slice_call
 *
 * SLICE 操作的专用处理函数（SLICE_PUT/GET/DEL）
 */
static Datum falcon_batch_slice_call(int32_t operation_type, uint32_t count, char *paramBuffer, int64_t signature)
{
    printf("[debug] falcon_batch_slice_call: ENTRY, operation_type=%d(%s), count=%u\n",
           operation_type, MetaServiceTypeNameFromProto(operation_type), count);
    fflush(stdout);

    char *p = paramBuffer;

    /* 1. 构造 SliceProcessInfo 数组 */
    void *data = palloc((sizeof(SliceProcessInfoData) + sizeof(SliceProcessInfo)) * count);
    SliceProcessInfoData *sliceInfoArray = data;
    SliceProcessInfo *sliceArray = (SliceProcessInfo *)(sliceInfoArray + count);

    /* 使用 protobuf 解析请求 */
    for (uint32_t i = 0; i < count; i++) {
        sliceArray[i] = &sliceInfoArray[i];
        memset(&sliceInfoArray[i], 0, sizeof(SliceProcessInfoData));

        /* 读取 protobuf 长度前缀 */
        uint32_t pbLen = *(uint32_t*)p;
        p += sizeof(uint32_t);
        char *pbData = p;
        p += pbLen;

        /* 根据操作类型解析 protobuf */
        if (operation_type == 23) {  /* SLICE_PUT */
            ProtobufParseSlicePutRequest(pbData, pbLen, &sliceInfoArray[i]);
        } else if (operation_type == 24 || operation_type == 25) {  /* SLICE_GET / SLICE_DEL */
            ProtobufParseSliceIndexRequest(pbData, pbLen, &sliceInfoArray[i]);
        }
    }

    /* 2. 调用 Handle 函数 */
    switch (operation_type) {
        case 23:  /* SLICE_PUT */
            FalconSlicePutHandle(sliceArray, count);
            break;
        case 24:  /* SLICE_GET */
            FalconSliceGetHandle(sliceArray, count);
            break;
        case 25:  /* SLICE_DEL */
            FalconSliceDelHandle(sliceArray, count);
            break;
        default:
            FALCON_ELOG_ERROR(ARGUMENT_ERROR, "Unsupported SLICE operation");
    }

    /* 3. 使用 protobuf 序列化响应 */
    char **respBuffers = palloc(sizeof(char*) * count);
    size_t *respSizes = palloc(sizeof(size_t) * count);
    size_t totalSize = 0;

    for (uint32_t i = 0; i < count; i++) {
        char *respBuffer = NULL;
        size_t respSize = 0;

        if (operation_type == 23 || operation_type == 25) {  /* SLICE_PUT / SLICE_DEL */
            respSize = ProtobufSerializeSimpleResponse(sliceInfoArray[i].errorCode, &respBuffer);
        } else if (operation_type == 24) {  /* SLICE_GET */
            respSize = ProtobufSerializeSliceGetResponse(&sliceInfoArray[i], &respBuffer);
        }

        respBuffers[i] = respBuffer;
        respSizes[i] = respSize;
        totalSize += sizeof(uint32_t) + respSize;
    }

    /* 4. 分配共享内存 */
    FalconShmemAllocator *allocator = &FalconConnectionPoolShmemAllocator;
    uint64_t responseShmemShift = FalconShmemAllocatorMalloc(allocator, totalSize);
    if (responseShmemShift == 0)
        FALCON_ELOG_ERROR_EXTENDED(PROGRAM_ERROR, "FalconShmemAllocatorMalloc failed. Size: %lu.", totalSize);

    char *responseBuffer = FALCON_SHMEM_ALLOCATOR_GET_POINTER(allocator, responseShmemShift);
    FALCON_SHMEM_ALLOCATOR_SET_SIGNATURE(responseBuffer, signature);

    /* 5. 写入响应 */
    char *resp_p = responseBuffer;
    for (uint32_t i = 0; i < count; i++) {
        *(uint32_t*)resp_p = (uint32_t)respSizes[i];
        resp_p += sizeof(uint32_t);
        memcpy(resp_p, respBuffers[i], respSizes[i]);
        resp_p += respSizes[i];
        ProtobufSerializerFreeBuffer(respBuffers[i]);
    }

    pfree(respBuffers);
    pfree(respSizes);

    printf("[debug] falcon_batch_slice_call: EXIT, response_size=%zu\n", totalSize);
    fflush(stdout);

    PG_RETURN_INT64(responseShmemShift);
}
