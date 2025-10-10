/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

#include "falcon_meta_rpc.pb.h"
#include <string>
#include <cstring>

extern "C" {
#include "postgres.h"
#include "metadb/meta_protobuf_serializer.h"
}

size_t ProtobufSerializeSimpleResponse(int32_t errorCode, char **outBuffer)
{
    falcon::meta_proto::SimpleResponseData response;
    response.set_error_code(errorCode);

    size_t size = response.ByteSizeLong();
    *outBuffer = new char[size];
    if (!response.SerializeToArray(*outBuffer, size)) {
        delete[] *outBuffer;
        *outBuffer = nullptr;
        return 0;
    }
    return size;
}

size_t ProtobufSerializeCreateResponse(MetaProcessInfoData *info, char **outBuffer)
{
    falcon::meta_proto::CreateResponseData response;
    response.set_error_code(info->errorCode);
    response.set_st_ino(info->inodeId);
    response.set_node_id(info->node_id);
    response.set_st_dev(info->st_dev);
    response.set_st_mode(info->st_mode);
    response.set_st_nlink(info->st_nlink);
    response.set_st_uid(info->st_uid);
    response.set_st_gid(info->st_gid);
    response.set_st_rdev(info->st_rdev);
    response.set_st_size(info->st_size);
    response.set_st_blksize(info->st_blksize);
    response.set_st_blocks(info->st_blocks);
    response.set_st_atim(info->st_atim);
    response.set_st_mtim(info->st_mtim);
    response.set_st_ctim(info->st_ctim);

    size_t size = response.ByteSizeLong();
    *outBuffer = new char[size];
    if (!response.SerializeToArray(*outBuffer, size)) {
        delete[] *outBuffer;
        *outBuffer = nullptr;
        return 0;
    }
    return size;
}

size_t ProtobufSerializeStatResponse(MetaProcessInfoData *info, char **outBuffer)
{
    falcon::meta_proto::StatResponseData response;
    response.set_error_code(info->errorCode);
    response.set_st_ino(info->inodeId);
    response.set_st_dev(info->st_dev);
    response.set_st_mode(info->st_mode);
    response.set_st_nlink(info->st_nlink);
    response.set_st_uid(info->st_uid);
    response.set_st_gid(info->st_gid);
    response.set_st_rdev(info->st_rdev);
    response.set_st_size(info->st_size);
    response.set_st_blksize(info->st_blksize);
    response.set_st_blocks(info->st_blocks);
    response.set_st_atim(info->st_atim);
    response.set_st_mtim(info->st_mtim);
    response.set_st_ctim(info->st_ctim);

    size_t size = response.ByteSizeLong();
    *outBuffer = new char[size];
    if (!response.SerializeToArray(*outBuffer, size)) {
        delete[] *outBuffer;
        *outBuffer = nullptr;
        return 0;
    }
    return size;
}

size_t ProtobufSerializeOpenResponse(MetaProcessInfoData *info, char **outBuffer)
{
    falcon::meta_proto::OpenResponseData response;
    response.set_error_code(info->errorCode);
    response.set_st_ino(info->inodeId);
    response.set_node_id(info->node_id);
    response.set_st_dev(info->st_dev);
    response.set_st_mode(info->st_mode);
    response.set_st_nlink(info->st_nlink);
    response.set_st_uid(info->st_uid);
    response.set_st_gid(info->st_gid);
    response.set_st_rdev(info->st_rdev);
    response.set_st_size(info->st_size);
    response.set_st_blksize(info->st_blksize);
    response.set_st_blocks(info->st_blocks);
    response.set_st_atim(info->st_atim);
    response.set_st_mtim(info->st_mtim);
    response.set_st_ctim(info->st_ctim);

    size_t size = response.ByteSizeLong();
    *outBuffer = new char[size];
    if (!response.SerializeToArray(*outBuffer, size)) {
        delete[] *outBuffer;
        *outBuffer = nullptr;
        return 0;
    }
    return size;
}

size_t ProtobufSerializeUnlinkResponse(MetaProcessInfoData *info, char **outBuffer)
{
    falcon::meta_proto::UnlinkResponseData response;
    response.set_error_code(info->errorCode);
    response.set_st_ino(info->inodeId);
    response.set_st_size(info->st_size);
    response.set_node_id(info->node_id);

    size_t size = response.ByteSizeLong();
    *outBuffer = new char[size];
    if (!response.SerializeToArray(*outBuffer, size)) {
        delete[] *outBuffer;
        *outBuffer = nullptr;
        return 0;
    }
    return size;
}

size_t ProtobufSerializeOpenDirResponse(MetaProcessInfoData *info, char **outBuffer)
{
    falcon::meta_proto::OpenDirResponseData response;
    response.set_error_code(info->errorCode);
    response.set_st_ino(info->inodeId);

    size_t size = response.ByteSizeLong();
    *outBuffer = new char[size];
    if (!response.SerializeToArray(*outBuffer, size)) {
        delete[] *outBuffer;
        *outBuffer = nullptr;
        return 0;
    }
    return size;
}

size_t ProtobufSerializeReadDirResponse(MetaProcessInfoData *info, char **outBuffer)
{
    falcon::meta_proto::ReadDirResponseData response;
    response.set_error_code(info->errorCode);
    response.set_last_shard_index(info->readDirLastShardIndex);
    if (info->readDirLastFileName != nullptr) {
        response.set_last_file_name(info->readDirLastFileName);
    }

    for (int i = 0; i < info->readDirResultCount; ++i) {
        auto* entry = response.add_entries();
        entry->set_file_name(info->readDirResultList[i]->fileName);
        entry->set_st_mode(info->readDirResultList[i]->mode);
    }

    size_t size = response.ByteSizeLong();
    *outBuffer = new char[size];
    if (!response.SerializeToArray(*outBuffer, size)) {
        delete[] *outBuffer;
        *outBuffer = nullptr;
        return 0;
    }
    return size;
}

size_t ProtobufSerializeKvGetResponse(KvMetaProcessInfoData *info, char **outBuffer)
{
    falcon::meta_proto::KvGetResponseData response;
    response.set_error_code(info->errorCode);

    if (info->errorCode == SUCCESS) {
        if (info->userkey != nullptr) {
            response.set_key(info->userkey);
        }
        response.set_value_len(info->valuelen);
        response.set_slice_num(info->slicenum);

        for (int i = 0; i < info->slicenum; ++i) {
            auto* slice = response.add_slices();
            slice->set_value_key(info->valuekey[i]);
            slice->set_location(info->location[i]);
            slice->set_size(info->slicelen[i]);
        }
    }

    size_t size = response.ByteSizeLong();
    *outBuffer = new char[size];
    if (!response.SerializeToArray(*outBuffer, size)) {
        delete[] *outBuffer;
        *outBuffer = nullptr;
        return 0;
    }
    return size;
}

size_t ProtobufSerializeSliceGetResponse(SliceProcessInfoData *info, char **outBuffer)
{
    falcon::meta_proto::SliceGetResponseData response;
    response.set_error_code(info->errorCode);
    response.set_slice_num(info->count);

    for (uint32_t i = 0; i < info->count; ++i) {
        auto* slice = response.add_slices();
        slice->set_inode_id(info->inodeIds[i]);
        slice->set_chunk_id(info->chunkIds[i]);
        slice->set_slice_id(info->sliceIds[i]);
        slice->set_slice_size(info->sliceSizes[i]);
        slice->set_slice_offset(info->sliceOffsets[i]);
        slice->set_slice_len(info->sliceLens[i]);
        slice->set_slice_loc1(info->sliceLoc1s[i]);
        slice->set_slice_loc2(info->sliceloc2s[i]);
    }

    size_t size = response.ByteSizeLong();
    *outBuffer = new char[size];
    if (!response.SerializeToArray(*outBuffer, size)) {
        delete[] *outBuffer;
        *outBuffer = nullptr;
        return 0;
    }
    return size;
}

size_t ProtobufSerializeFetchSliceIdResponse(SliceIdProcessInfoData *info, char **outBuffer)
{
    falcon::meta_proto::FetchSliceIdResponseData response;
    response.set_error_code(info->errorCode);
    response.set_start(info->start);
    response.set_end(info->end);

    size_t size = response.ByteSizeLong();
    *outBuffer = new char[size];
    if (!response.SerializeToArray(*outBuffer, size)) {
        delete[] *outBuffer;
        *outBuffer = nullptr;
        return 0;
    }
    return size;
}

void ProtobufSerializerFreeBuffer(char *buffer)
{
    delete[] buffer;
}

// Helper to allocate string in PostgreSQL memory context
static char* AllocPgString(const std::string& str)
{
    char* result = (char*)palloc(str.length() + 1);
    memcpy(result, str.c_str(), str.length());
    result[str.length()] = '\0';
    return result;
}

char* ProtobufParsePathOnlyRequest(const char *data, size_t size)
{
    falcon::meta_proto::PathOnlyRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return nullptr;
    }
    return AllocPgString(req.path());
}

int ProtobufParseCloseRequest(const char *data, size_t size, MetaProcessInfoData *info)
{
    falcon::meta_proto::CloseRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return -1;
    }
    info->path = AllocPgString(req.path());
    info->st_size = req.st_size();
    info->st_mtim = req.st_mtim();
    info->node_id = req.node_id();
    return 0;
}

int ProtobufParseReadDirRequest(const char *data, size_t size, MetaProcessInfoData *info)
{
    falcon::meta_proto::ReadDirRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return -1;
    }
    info->path = AllocPgString(req.path());
    info->readDirMaxReadCount = req.max_read_count();
    info->readDirLastShardIndex = req.last_shard_index();
    info->readDirLastFileName = AllocPgString(req.last_file_name());
    return 0;
}

int ProtobufParseMkdirSubMkdirRequest(const char *data, size_t size, MetaProcessInfoData *info)
{
    falcon::meta_proto::MkdirSubMkdirRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return -1;
    }
    info->parentId = req.parent_id();
    info->name = AllocPgString(req.name());
    info->inodeId = req.inode_id();
    return 0;
}

int ProtobufParseMkdirSubCreateRequest(const char *data, size_t size, MetaProcessInfoData *info)
{
    falcon::meta_proto::MkdirSubCreateRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return -1;
    }
    info->parentId_partId = req.parent_id_part_id();
    info->name = AllocPgString(req.name());
    info->inodeId = req.inode_id();
    info->st_mode = req.st_mode();
    info->st_mtim = req.st_mtim();
    info->st_size = req.st_size();
    return 0;
}

int ProtobufParseRmdirSubRmdirRequest(const char *data, size_t size, MetaProcessInfoData *info)
{
    falcon::meta_proto::RmdirSubRmdirRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return -1;
    }
    info->parentId = req.parent_id();
    info->name = AllocPgString(req.name());
    return 0;
}

int ProtobufParseRmdirSubUnlinkRequest(const char *data, size_t size, MetaProcessInfoData *info)
{
    falcon::meta_proto::RmdirSubUnlinkRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return -1;
    }
    info->parentId_partId = req.parent_id_part_id();
    info->name = AllocPgString(req.name());
    return 0;
}

int ProtobufParseRenameRequest(const char *data, size_t size, MetaProcessInfoData *info)
{
    falcon::meta_proto::RenameRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return -1;
    }
    info->path = AllocPgString(req.src());
    info->dstPath = AllocPgString(req.dst());
    return 0;
}

int ProtobufParseRenameSubRenameLocallyRequest(const char *data, size_t size, MetaProcessInfoData *info)
{
    falcon::meta_proto::RenameSubRenameLocallyRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return -1;
    }
    info->parentId = req.src_parent_id();
    info->parentId_partId = req.src_parent_id_part_id();
    info->name = AllocPgString(req.src_name());
    info->dstParentId = req.dst_parent_id();
    info->dstParentIdPartId = req.dst_parent_id_part_id();
    info->dstName = AllocPgString(req.dst_name());
    info->targetIsDirectory = req.target_is_directory() ? 1 : 0;
    info->inodeId = req.directory_inode_id();
    info->srcLockOrder = req.src_lock_order();
    return 0;
}

int ProtobufParseRenameSubCreateRequest(const char *data, size_t size, MetaProcessInfoData *info)
{
    falcon::meta_proto::RenameSubCreateRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return -1;
    }
    info->parentId_partId = req.parentid_partid();
    info->name = AllocPgString(req.name());
    info->inodeId = req.st_ino();
    info->st_dev = req.st_dev();
    info->st_mode = req.st_mode();
    info->st_nlink = req.st_nlink();
    info->st_uid = req.st_uid();
    info->st_gid = req.st_gid();
    info->st_rdev = req.st_rdev();
    info->st_size = req.st_size();
    info->st_blksize = req.st_blksize();
    info->st_blocks = req.st_blocks();
    info->st_atim = req.st_atim();
    info->st_mtim = req.st_mtim();
    info->st_ctim = req.st_ctim();
    info->node_id = req.node_id();
    return 0;
}

int ProtobufParseUtimeNsRequest(const char *data, size_t size, MetaProcessInfoData *info)
{
    falcon::meta_proto::UtimeNsRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return -1;
    }
    info->path = AllocPgString(req.path());
    info->st_atim = req.st_atim();
    info->st_mtim = req.st_mtim();
    return 0;
}

int ProtobufParseChownRequest(const char *data, size_t size, MetaProcessInfoData *info)
{
    falcon::meta_proto::ChownRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return -1;
    }
    info->path = AllocPgString(req.path());
    info->st_uid = req.st_uid();
    info->st_gid = req.st_gid();
    return 0;
}

int ProtobufParseChmodRequest(const char *data, size_t size, MetaProcessInfoData *info)
{
    falcon::meta_proto::ChmodRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return -1;
    }
    info->path = AllocPgString(req.path());
    info->st_mode = req.st_mode();
    return 0;
}

int ProtobufParseKvPutRequest(const char *data, size_t size, KvMetaProcessInfoData *info)
{
    falcon::meta_proto::KvPutRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return -1;
    }
    info->userkey = AllocPgString(req.key());
    info->valuelen = req.value_len();
    info->slicenum = req.slice_num();

    info->valuekey = (uint64_t*)palloc(info->slicenum * sizeof(uint64_t));
    info->location = (uint64_t*)palloc(info->slicenum * sizeof(uint64_t));
    info->slicelen = (uint32_t*)palloc(info->slicenum * sizeof(uint32_t));

    for (int i = 0; i < req.slices_size(); ++i) {
        info->valuekey[i] = req.slices(i).value_key();
        info->location[i] = req.slices(i).location();
        info->slicelen[i] = req.slices(i).size();
    }
    return 0;
}

int ProtobufParseKvKeyOnlyRequest(const char *data, size_t size, KvMetaProcessInfoData *info)
{
    falcon::meta_proto::KvKeyOnlyRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return -1;
    }
    info->userkey = AllocPgString(req.key());
    return 0;
}

char* ProtobufParsePlainCommandRequest(const char *data, size_t size)
{
    falcon::meta_proto::PlainCommandRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return nullptr;
    }
    return AllocPgString(req.command());
}

int ProtobufParseSliceIndexRequest(const char *data, size_t size, SliceProcessInfoData *info)
{
    falcon::meta_proto::SliceIndexRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return -1;
    }
    info->name = AllocPgString(req.filename());
    info->inputInodeid = req.inode_id();
    info->inputChunkid = req.chunk_id();
    return 0;
}

int ProtobufParseSlicePutRequest(const char *data, size_t size, SliceProcessInfoData *info)
{
    falcon::meta_proto::SlicePutRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return -1;
    }
    info->name = AllocPgString(req.filename());
    info->count = req.slice_num();

    info->inodeIds = (uint64_t*)palloc(sizeof(uint64_t) * info->count);
    info->chunkIds = (uint32_t*)palloc(sizeof(uint32_t) * info->count);
    info->sliceIds = (uint64_t*)palloc(sizeof(uint64_t) * info->count);
    info->sliceSizes = (uint32_t*)palloc(sizeof(uint32_t) * info->count);
    info->sliceOffsets = (uint32_t*)palloc(sizeof(uint32_t) * info->count);
    info->sliceLens = (uint32_t*)palloc(sizeof(uint32_t) * info->count);
    info->sliceLoc1s = (uint32_t*)palloc(sizeof(uint32_t) * info->count);
    info->sliceloc2s = (uint32_t*)palloc(sizeof(uint32_t) * info->count);

    for (int i = 0; i < req.slices_size(); ++i) {
        info->inodeIds[i] = req.slices(i).inode_id();
        info->chunkIds[i] = req.slices(i).chunk_id();
        info->sliceIds[i] = req.slices(i).slice_id();
        info->sliceSizes[i] = req.slices(i).slice_size();
        info->sliceOffsets[i] = req.slices(i).slice_offset();
        info->sliceLens[i] = req.slices(i).slice_len();
        info->sliceLoc1s[i] = req.slices(i).slice_loc1();
        info->sliceloc2s[i] = req.slices(i).slice_loc2();
    }
    return 0;
}

int ProtobufParseFetchSliceIdRequest(const char *data, size_t size, SliceIdProcessInfoData *info)
{
    falcon::meta_proto::FetchSliceIdRequestData req;
    if (!req.ParseFromArray(data, size)) {
        return -1;
    }
    info->count = req.count();
    info->type = req.type();
    return 0;
}
