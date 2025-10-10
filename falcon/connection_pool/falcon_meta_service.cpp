/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

#include "connection_pool/falcon_meta_service_internal.h"
#include "connection_pool/pg_connection.h"
#include "connection_pool/connection_pool_config.h"
#include "connection_pool/pg_connection_pool.h"
#include "falcon_meta_rpc.pb.h"
#include "connection_pool/task.h"
#include <brpc/controller.h>
#include <google/protobuf/stubs/callback.h>
#include <libpq-fe.h>
#include <sstream>
#include <cstring>
#include <thread>
#include <cstdlib>
#include <vector>
#include <mutex>

namespace falcon {
namespace meta_service {

static void HandleFalconMetaResponse(brpc::Controller* cntl,
                                     falcon::meta_proto::Empty* proto_response,
                                     AsyncFalconMetaServiceJob* original_job)
{
    printf("[debug] HandleFalconMetaResponse: ENTRY, job=%p\n", original_job);
    fflush(stdout);

    FalconMetaServiceResponse& response = original_job->GetResponse();
    response.opcode = original_job->GetRequest().operation;

    printf("[debug] HandleFalconMetaResponse: opcode=%d(%s), cntl->Failed()=%d\n",
           response.opcode, FalconMetaOperationTypeName(response.opcode), cntl->Failed());
    fflush(stdout);

    if (cntl->Failed()) {
        printf("[debug] HandleFalconMetaResponse: BRPC call failed, error=%s\n",
               cntl->ErrorText().c_str());
        fflush(stdout);
        response.status = -1;
        response.data = nullptr;
    } else {
        printf("[debug] HandleFalconMetaResponse: Deserializing response, attachment_size=%lu\n",
               cntl->response_attachment().size());
        fflush(stdout);

        if (!FalconMetaServiceSerializer::DeserializeResponseFromProtobuf(
                cntl->response_attachment(),
                &response,
                original_job->GetRequest().operation)) {
            printf("[debug] HandleFalconMetaResponse: Deserialization FAILED\n");
            fflush(stdout);
            response.status = -1;
        } else {
            printf("[debug] HandleFalconMetaResponse: Deserialization SUCCESS, status=%d\n",
                   response.status);
            fflush(stdout);
        }
    }

    printf("[debug] HandleFalconMetaResponse: Calling original_job->Done()\n");
    fflush(stdout);

    original_job->Done();

    printf("[debug] HandleFalconMetaResponse: Cleanup and EXIT\n");
    fflush(stdout);

    delete cntl;
    delete proto_response;
    delete original_job;
}

}  // namespace meta_service
}  // namespace falcon

namespace falcon {
namespace meta_service {

FalconMetaService* FalconMetaService::instance = nullptr;
std::mutex FalconMetaService::instanceMutex;

FalconMetaService::FalconMetaService() : initialized(false)
{
}

FalconMetaService* FalconMetaService::Instance()
{
    std::lock_guard<std::mutex> lock(instanceMutex);
    if (instance == nullptr) {
        instance = new FalconMetaService();
    }
    return instance;
}

bool FalconMetaService::Init(int port, int pool_size)
{
    printf("[FalconMetaService::Init] DEBUG: Entry - port=%d, pool_size=%d\n", port, pool_size);
    fflush(stdout);

    std::lock_guard<std::mutex> lock(instanceMutex);

    printf("[FalconMetaService::Init] DEBUG: Lock acquired, initialized=%d\n", initialized);
    fflush(stdout);

    if (initialized) {
        printf("[FalconMetaService] WARNING: Already initialized, skipping re-initialization\n");
        fflush(stdout);
        return true;
    }

    printf("[FalconMetaService::Init] DEBUG: Checking port validity: %d\n", port);
    fflush(stdout);

    if (port <= 0 || port > 65535) {
        printf("[FalconMetaService] ERROR: Invalid port number: %d\n", port);
        fflush(stdout);
        return false;
    }

    printf("[FalconMetaService::Init] DEBUG: Checking pool_size validity: %d\n", pool_size);
    fflush(stdout);

    if (pool_size <= 0) {
        printf("[FalconMetaService] ERROR: Invalid pool size: %d\n", pool_size);
        fflush(stdout);
        return false;
    }

    printf("[FalconMetaService::Init] DEBUG: Getting USER environment variable\n");
    fflush(stdout);

    // 获取 USER 环境变量
    char* user_name = getenv("USER");
    if (!user_name) {
        printf("[FalconMetaService] ERROR: Cannot get USER from environment\n");
        fflush(stdout);
        return false;
    }

    printf("[FalconMetaService::Init] DEBUG: USER=%s, creating PGConnectionPool\n", user_name);
    fflush(stdout);

    try {
        printf("[FalconMetaService::Init] DEBUG: About to create PGConnectionPool with port=%d, user=%s, pool_size=%d\n",
               port, user_name, pool_size);
        fflush(stdout);

        pgConnectionPool = std::make_shared<PGConnectionPool>(
            port, user_name, pool_size, 20, 400);

        printf("[FalconMetaService] Initialized with: port=%d, user=%s, poolSize=%d\n",
               port, user_name, pool_size);
        fflush(stdout);

        initialized = true;
        printf("[FalconMetaService::Init] DEBUG: Success, initialized set to true\n");
        fflush(stdout);
        return true;
    } catch (const std::exception& e) {
        printf("[FalconMetaService] ERROR: Failed to initialize connection pool: %s\n", e.what());
        printf("[FalconMetaService::Init] DEBUG: Exception caught, returning false\n");
        fflush(stdout);
        return false;
    }
}

FalconMetaService::~FalconMetaService()
{
    if (pgConnectionPool) {
        printf("[FalconMetaService] Stopping internal PGConnectionPool\n");
        fflush(stdout);
        pgConnectionPool->Stop();
        pgConnectionPool.reset();
    }
}

int FalconMetaService::DispatchFalconMetaServiceJob(AsyncFalconMetaServiceJob* job)
{
    printf("[debug] DispatchFalconMetaServiceJob: ENTRY, job=%p\n", job);
    fflush(stdout);

    if (pgConnectionPool == nullptr) {
        printf("[debug] DispatchFalconMetaServiceJob: ERROR - pgConnectionPool is NULL\n");
        fflush(stdout);
        if (job != nullptr) {
            job->GetResponse().status = -1;
            job->Done();
            delete job;
        }
        return -1;
    }

    FalconMetaServiceRequest& request = job->GetRequest();
    printf("[debug] DispatchFalconMetaServiceJob: opcode=%d(%s)\n", request.operation, FalconMetaOperationTypeName(request.operation));
    fflush(stdout);

    // 1. 创建 BRPC Controller 和 Protobuf 请求
    brpc::Controller* cntl = new brpc::Controller();
    falcon::meta_proto::MetaRequest* proto_request = new falcon::meta_proto::MetaRequest();
    falcon::meta_proto::Empty* proto_response = new falcon::meta_proto::Empty();

    printf("[debug] DispatchFalconMetaServiceJob: Created BRPC objects, cntl=%p\n", cntl);
    fflush(stdout);

    // 2. 序列化请求参数为 Protobuf 格式
    if (!FalconMetaServiceSerializer::SerializeRequestToProtobuf(
            request, proto_request, &cntl->request_attachment())) {
        printf("[debug] DispatchFalconMetaServiceJob: ERROR - SerializeRequestToProtobuf FAILED\n");
        fflush(stdout);
        job->GetResponse().status = -1;
        job->Done();
        delete job;
        delete cntl;
        delete proto_request;
        delete proto_response;
        return -1;
    }

    printf("[debug] DispatchFalconMetaServiceJob: Serialization SUCCESS, attachment_size=%lu\n",
           cntl->request_attachment().size());
    fflush(stdout);

    google::protobuf::Closure* done_callback = brpc::NewCallback(
        &HandleFalconMetaResponse, cntl, proto_response, job);

    printf("[debug] DispatchFalconMetaServiceJob: Created callback=%p\n", done_callback);
    fflush(stdout);

    falcon::meta_proto::AsyncMetaServiceJob* brpc_job =
        new falcon::meta_proto::AsyncMetaServiceJob(cntl, proto_request, proto_response, done_callback);

    printf("[debug] DispatchFalconMetaServiceJob: Dispatching brpc_job=%p to PGConnectionPool\n",
           brpc_job);
    fflush(stdout);

    pgConnectionPool->DispatchAsyncMetaServiceJob(brpc_job);

    printf("[debug] DispatchFalconMetaServiceJob: EXIT SUCCESS\n");
    fflush(stdout);

    return 0;
}

// SubmitFalconMetaRequest成员方法
int FalconMetaService::SubmitFalconMetaRequest(const FalconMetaServiceRequest& request,
                                               FalconMetaServiceCallback callback,
                                               void* user_context)
{
    printf("[debug] FalconMetaService::SubmitFalconMetaRequest: ENTRY, opcode=%d(%s), callback_valid=%d\n",
           request.operation, FalconMetaOperationTypeName(request.operation), callback ? 1 : 0);
    fflush(stdout);

    if (!initialized) {
        printf("[FalconMetaService] ERROR: Service not initialized. Call Init() first.\n");
        fflush(stdout);
        return -1;
    }

    AsyncFalconMetaServiceJob* job = new AsyncFalconMetaServiceJob(request, callback, user_context);

    int ret = DispatchFalconMetaServiceJob(job);

    printf("[debug] FalconMetaService::SubmitFalconMetaRequest: EXIT, ret=%d\n", ret);
    fflush(stdout);

    return ret;
}

// 将 FalconMetaOperationType 转换为 falcon::meta_proto::MetaServiceType
static falcon::meta_proto::MetaServiceType ConvertToProtoType(FalconMetaOperationType op)
{
    switch (op) {
        case DFC_PUT_KEY_META: return falcon::meta_proto::MetaServiceType::KV_PUT;
        case DFC_GET_KV_META: return falcon::meta_proto::MetaServiceType::KV_GET;
        case DFC_DELETE_KV_META: return falcon::meta_proto::MetaServiceType::KV_DEL;
        case DFC_MKDIR: return falcon::meta_proto::MetaServiceType::MKDIR;
        case DFC_MKDIR_SUB_MKDIR: return falcon::meta_proto::MetaServiceType::MKDIR_SUB_MKDIR;
        case DFC_MKDIR_SUB_CREATE: return falcon::meta_proto::MetaServiceType::MKDIR_SUB_CREATE;
        case DFC_CREATE: return falcon::meta_proto::MetaServiceType::CREATE;
        case DFC_STAT: return falcon::meta_proto::MetaServiceType::STAT;
        case DFC_OPEN: return falcon::meta_proto::MetaServiceType::OPEN;
        case DFC_CLOSE: return falcon::meta_proto::MetaServiceType::CLOSE;
        case DFC_UNLINK: return falcon::meta_proto::MetaServiceType::UNLINK;
        case DFC_READDIR: return falcon::meta_proto::MetaServiceType::READDIR;
        case DFC_OPENDIR: return falcon::meta_proto::MetaServiceType::OPENDIR;
        case DFC_RMDIR: return falcon::meta_proto::MetaServiceType::RMDIR;
        case DFC_RMDIR_SUB_RMDIR: return falcon::meta_proto::MetaServiceType::RMDIR_SUB_RMDIR;
        case DFC_RMDIR_SUB_UNLINK: return falcon::meta_proto::MetaServiceType::RMDIR_SUB_UNLINK;
        case DFC_RENAME: return falcon::meta_proto::MetaServiceType::RENAME;
        case DFC_RENAME_SUB_RENAME_LOCALLY: return falcon::meta_proto::MetaServiceType::RENAME_SUB_RENAME_LOCALLY;
        case DFC_RENAME_SUB_CREATE: return falcon::meta_proto::MetaServiceType::RENAME_SUB_CREATE;
        case DFC_UTIMENS: return falcon::meta_proto::MetaServiceType::UTIMENS;
        case DFC_CHOWN: return falcon::meta_proto::MetaServiceType::CHOWN;
        case DFC_CHMOD: return falcon::meta_proto::MetaServiceType::CHMOD;
        case DFC_SLICE_PUT: return falcon::meta_proto::MetaServiceType::SLICE_PUT;
        case DFC_SLICE_GET: return falcon::meta_proto::MetaServiceType::SLICE_GET;
        case DFC_SLICE_DEL: return falcon::meta_proto::MetaServiceType::SLICE_DEL;
        case DFC_FETCH_SLICE_ID: return falcon::meta_proto::MetaServiceType::FETCH_SLICE_ID;
        default: return falcon::meta_proto::MetaServiceType::PLAIN_COMMAND;
    }
}

bool FalconMetaServiceSerializer::SerializeRequestToProtobuf(
    const FalconMetaServiceRequest& request,
    falcon::meta_proto::MetaRequest* proto_request,
    butil::IOBuf* attachment)
{
    printf("[debug] [SerializeRequestToProtobuf] ENTRY: operation=%d(%s)\n",
           request.operation, FalconMetaOperationTypeName(request.operation));
    fflush(stdout);

    falcon::meta_proto::MetaServiceType proto_type = ConvertToProtoType(request.operation);
    proto_request->add_type(proto_type);

    if (proto_type == falcon::meta_proto::MKDIR ||
        proto_type == falcon::meta_proto::CREATE ||
        proto_type == falcon::meta_proto::STAT ||
        proto_type == falcon::meta_proto::OPEN ||
        proto_type == falcon::meta_proto::CLOSE ||
        proto_type == falcon::meta_proto::UNLINK) {
        proto_request->set_allow_batch_with_others(true);
    }

    proto_request->set_format(falcon::meta_proto::SerializationFormat::PROTOBUF);

    std::string serialized_data;

    switch (request.operation) {
        case DFC_MKDIR:
        case DFC_CREATE:
        case DFC_STAT:
        case DFC_OPEN:
        case DFC_UNLINK:
        case DFC_OPENDIR:
        case DFC_RMDIR: {
            const PathOnlyParam* param = meta_param_helper::Get<PathOnlyParam>(request.file_params);
            if (!param) return false;
            falcon::meta_proto::PathOnlyRequestData pb_req;
            pb_req.set_path(param->path);
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        case DFC_CLOSE: {
            const CloseParam* param = meta_param_helper::Get<CloseParam>(request.file_params);
            if (!param) return false;
            falcon::meta_proto::CloseRequestData pb_req;
            pb_req.set_path(param->path);
            pb_req.set_st_size(param->st_size);
            pb_req.set_st_mtim(param->st_mtim);
            pb_req.set_node_id(param->node_id);
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        case DFC_READDIR: {
            const ReadDirParam* param = meta_param_helper::Get<ReadDirParam>(request.file_params);
            if (!param) return false;
            falcon::meta_proto::ReadDirRequestData pb_req;
            pb_req.set_path(param->path);
            pb_req.set_max_read_count(param->max_read_count);
            pb_req.set_last_shard_index(param->last_shard_index);
            pb_req.set_last_file_name(param->last_file_name);
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        case DFC_MKDIR_SUB_MKDIR: {
            const MkdirSubMkdirParam* param = meta_param_helper::Get<MkdirSubMkdirParam>(request.file_params);
            if (!param) return false;
            falcon::meta_proto::MkdirSubMkdirRequestData pb_req;
            pb_req.set_parent_id(param->parent_id);
            pb_req.set_name(param->name);
            pb_req.set_inode_id(param->inode_id);
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        case DFC_MKDIR_SUB_CREATE: {
            const MkdirSubCreateParam* param = meta_param_helper::Get<MkdirSubCreateParam>(request.file_params);
            if (!param) return false;
            falcon::meta_proto::MkdirSubCreateRequestData pb_req;
            pb_req.set_parent_id_part_id(param->parent_id_part_id);
            pb_req.set_name(param->name);
            pb_req.set_inode_id(param->inode_id);
            pb_req.set_st_mode(param->st_mode);
            pb_req.set_st_mtim(param->st_mtim);
            pb_req.set_st_size(param->st_size);
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        case DFC_RMDIR_SUB_RMDIR: {
            const RmdirSubRmdirParam* param = meta_param_helper::Get<RmdirSubRmdirParam>(request.file_params);
            if (!param) return false;
            falcon::meta_proto::RmdirSubRmdirRequestData pb_req;
            pb_req.set_parent_id(param->parent_id);
            pb_req.set_name(param->name);
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        case DFC_RMDIR_SUB_UNLINK: {
            const RmdirSubUnlinkParam* param = meta_param_helper::Get<RmdirSubUnlinkParam>(request.file_params);
            if (!param) return false;
            falcon::meta_proto::RmdirSubUnlinkRequestData pb_req;
            pb_req.set_parent_id_part_id(param->parent_id_part_id);
            pb_req.set_name(param->name);
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        case DFC_RENAME: {
            const RenameParam* param = meta_param_helper::Get<RenameParam>(request.file_params);
            if (!param) return false;
            falcon::meta_proto::RenameRequestData pb_req;
            pb_req.set_src(param->src);
            pb_req.set_dst(param->dst);
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        case DFC_RENAME_SUB_RENAME_LOCALLY: {
            const RenameSubRenameLocallyParam* param = meta_param_helper::Get<RenameSubRenameLocallyParam>(request.file_params);
            if (!param) return false;
            falcon::meta_proto::RenameSubRenameLocallyRequestData pb_req;
            pb_req.set_src_parent_id(param->src_parent_id);
            pb_req.set_src_parent_id_part_id(param->src_parent_id_part_id);
            pb_req.set_src_name(param->src_name);
            pb_req.set_dst_parent_id(param->dst_parent_id);
            pb_req.set_dst_parent_id_part_id(param->dst_parent_id_part_id);
            pb_req.set_dst_name(param->dst_name);
            pb_req.set_target_is_directory(param->target_is_directory);
            pb_req.set_directory_inode_id(param->directory_inode_id);
            pb_req.set_src_lock_order(param->src_lock_order);
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        case DFC_RENAME_SUB_CREATE: {
            const RenameSubCreateParam* param = meta_param_helper::Get<RenameSubCreateParam>(request.file_params);
            if (!param) return false;
            falcon::meta_proto::RenameSubCreateRequestData pb_req;
            pb_req.set_parentid_partid(param->parentid_partid);
            pb_req.set_name(param->name);
            pb_req.set_st_ino(param->st_ino);
            pb_req.set_st_dev(param->st_dev);
            pb_req.set_st_mode(param->st_mode);
            pb_req.set_st_nlink(param->st_nlink);
            pb_req.set_st_uid(param->st_uid);
            pb_req.set_st_gid(param->st_gid);
            pb_req.set_st_rdev(param->st_rdev);
            pb_req.set_st_size(param->st_size);
            pb_req.set_st_blksize(param->st_blksize);
            pb_req.set_st_blocks(param->st_blocks);
            pb_req.set_st_atim(param->st_atim);
            pb_req.set_st_mtim(param->st_mtim);
            pb_req.set_st_ctim(param->st_ctim);
            pb_req.set_node_id(param->node_id);
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        case DFC_UTIMENS: {
            const UtimeNsParam* param = meta_param_helper::Get<UtimeNsParam>(request.file_params);
            if (!param) return false;
            falcon::meta_proto::UtimeNsRequestData pb_req;
            pb_req.set_path(param->path);
            pb_req.set_st_atim(param->st_atim);
            pb_req.set_st_mtim(param->st_mtim);
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        case DFC_CHOWN: {
            const ChownParam* param = meta_param_helper::Get<ChownParam>(request.file_params);
            if (!param) return false;
            falcon::meta_proto::ChownRequestData pb_req;
            pb_req.set_path(param->path);
            pb_req.set_st_uid(param->st_uid);
            pb_req.set_st_gid(param->st_gid);
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        case DFC_CHMOD: {
            const ChmodParam* param = meta_param_helper::Get<ChmodParam>(request.file_params);
            if (!param) return false;
            falcon::meta_proto::ChmodRequestData pb_req;
            pb_req.set_path(param->path);
            pb_req.set_st_mode(param->st_mode);
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        case DFC_PUT_KEY_META: {
            falcon::meta_proto::KvPutRequestData pb_req;
            pb_req.set_key(request.kv_data.key);
            pb_req.set_value_len(request.kv_data.valueLen);
            pb_req.set_slice_num(request.kv_data.sliceNum);
            for (const auto& slice : request.kv_data.dataSlices) {
                auto* pb_slice = pb_req.add_slices();
                pb_slice->set_value_key(slice.value_key);
                pb_slice->set_location(slice.location);
                pb_slice->set_size(slice.size);
            }
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        case DFC_GET_KV_META:
        case DFC_DELETE_KV_META: {
            falcon::meta_proto::KvKeyOnlyRequestData pb_req;
            pb_req.set_key(request.kv_data.key);
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        case DFC_PLAIN_COMMAND: {
            const PlainCommandParam* param = meta_param_helper::Get<PlainCommandParam>(request.file_params);
            if (!param) return false;
            falcon::meta_proto::PlainCommandRequestData pb_req;
            pb_req.set_command(param->command);
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        case DFC_SLICE_GET:
        case DFC_SLICE_DEL: {
            const SliceIndexParam* param = meta_param_helper::Get<SliceIndexParam>(request.file_params);
            if (!param) return false;
            falcon::meta_proto::SliceIndexRequestData pb_req;
            pb_req.set_filename(param->filename);
            pb_req.set_inode_id(param->inodeid);
            pb_req.set_chunk_id(param->chunkid);
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        case DFC_SLICE_PUT: {
            const SliceInfoParam* param = meta_param_helper::Get<SliceInfoParam>(request.file_params);
            if (!param) return false;
            falcon::meta_proto::SlicePutRequestData pb_req;
            pb_req.set_filename(param->filename);
            pb_req.set_slice_num(param->slicenum);
            for (uint32_t i = 0; i < param->slicenum; ++i) {
                auto* pb_slice = pb_req.add_slices();
                pb_slice->set_inode_id(param->inodeid[i]);
                pb_slice->set_chunk_id(param->chunkid[i]);
                pb_slice->set_slice_id(param->sliceid[i]);
                pb_slice->set_slice_size(param->slicesize[i]);
                pb_slice->set_slice_offset(param->sliceoffset[i]);
                pb_slice->set_slice_len(param->slicelen[i]);
                pb_slice->set_slice_loc1(param->sliceloc1[i]);
                pb_slice->set_slice_loc2(param->sliceloc2[i]);
            }
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        case DFC_FETCH_SLICE_ID: {
            falcon::meta_proto::FetchSliceIdRequestData pb_req;
            pb_req.set_count(request.sliceid_param.count);
            pb_req.set_type(request.sliceid_param.type);
            pb_req.SerializeToString(&serialized_data);
            break;
        }

        default:
            return false;
    }

    // Write length prefix + serialized data
    uint32_t data_len = serialized_data.size();
    attachment->append(&data_len, sizeof(data_len));
    attachment->append(serialized_data);

    printf("[debug] [SerializeRequestToProtobuf] EXIT SUCCESS: serialized_size=%zu\n", serialized_data.size());
    fflush(stdout);

    return true;
}

bool FalconMetaServiceSerializer::DeserializeResponseFromProtobuf(
    const butil::IOBuf& attachment,
    FalconMetaServiceResponse* response,
    FalconMetaOperationType operation)
{
    printf("[debug] [DeserializeResponseFromProtobuf] ENTRY: operation=%d(%s), attachment_size=%lu\n",
           operation, FalconMetaOperationTypeName(operation), attachment.size());
    fflush(stdout);

    if (attachment.size() < sizeof(uint32_t)) {
        printf("[debug] [DeserializeResponseFromProtobuf] ERROR: attachment too small\n");
        fflush(stdout);
        return false;
    }

    std::vector<char> buffer(attachment.size());
    attachment.copy_to(&buffer[0], attachment.size());

    char* p = &buffer[0];
    response->opcode = operation;

    uint32_t pb_size = *(uint32_t*)p;
    p += sizeof(uint32_t);

    if (operation == DFC_MKDIR || operation == DFC_RMDIR ||
        operation == DFC_CLOSE || operation == DFC_RENAME ||
        operation == DFC_UTIMENS || operation == DFC_CHOWN ||
        operation == DFC_CHMOD || operation == DFC_PUT_KEY_META ||
        operation == DFC_DELETE_KV_META || operation == DFC_SLICE_PUT ||
        operation == DFC_SLICE_DEL) {

        falcon::meta_proto::SimpleResponseData pb_resp;
        if (!pb_resp.ParseFromArray(p, pb_size)) {
            printf("[debug] [DeserializeResponseFromProtobuf] ERROR: Failed to parse SimpleResponseData\n");
            fflush(stdout);
            return false;
        }
        response->status = pb_resp.error_code();
        response->data = nullptr;
        return true;
    }

    if (operation == DFC_CREATE) {
        falcon::meta_proto::CreateResponseData pb_resp;
        if (!pb_resp.ParseFromArray(p, pb_size)) {
            printf("[debug] [DeserializeResponseFromProtobuf] ERROR: Failed to parse CreateResponseData\n");
            fflush(stdout);
            return false;
        }
        response->status = pb_resp.error_code();

        CreateResponse* create_resp = new CreateResponse();
        create_resp->st_ino = pb_resp.st_ino();
        create_resp->node_id = pb_resp.node_id();
        create_resp->st_dev = pb_resp.st_dev();
        create_resp->st_mode = pb_resp.st_mode();
        create_resp->st_nlink = pb_resp.st_nlink();
        create_resp->st_uid = pb_resp.st_uid();
        create_resp->st_gid = pb_resp.st_gid();
        create_resp->st_rdev = pb_resp.st_rdev();
        create_resp->st_size = pb_resp.st_size();
        create_resp->st_blksize = pb_resp.st_blksize();
        create_resp->st_blocks = pb_resp.st_blocks();
        create_resp->st_atim = pb_resp.st_atim();
        create_resp->st_mtim = pb_resp.st_mtim();
        create_resp->st_ctim = pb_resp.st_ctim();
        response->data = create_resp;
        return true;
    }

    if (operation == DFC_STAT) {
        falcon::meta_proto::StatResponseData pb_resp;
        if (!pb_resp.ParseFromArray(p, pb_size)) {
            printf("[debug] [DeserializeResponseFromProtobuf] ERROR: Failed to parse StatResponseData\n");
            fflush(stdout);
            return false;
        }
        response->status = pb_resp.error_code();

        StatResponse* stat_resp = new StatResponse();
        stat_resp->st_ino = pb_resp.st_ino();
        stat_resp->st_dev = pb_resp.st_dev();
        stat_resp->st_mode = pb_resp.st_mode();
        stat_resp->st_nlink = pb_resp.st_nlink();
        stat_resp->st_uid = pb_resp.st_uid();
        stat_resp->st_gid = pb_resp.st_gid();
        stat_resp->st_rdev = pb_resp.st_rdev();
        stat_resp->st_size = pb_resp.st_size();
        stat_resp->st_blksize = pb_resp.st_blksize();
        stat_resp->st_blocks = pb_resp.st_blocks();
        stat_resp->st_atim = pb_resp.st_atim();
        stat_resp->st_mtim = pb_resp.st_mtim();
        stat_resp->st_ctim = pb_resp.st_ctim();
        response->data = stat_resp;
        return true;
    }

    if (operation == DFC_OPEN) {
        falcon::meta_proto::OpenResponseData pb_resp;
        if (!pb_resp.ParseFromArray(p, pb_size)) {
            printf("[debug] [DeserializeResponseFromProtobuf] ERROR: Failed to parse OpenResponseData\n");
            fflush(stdout);
            return false;
        }
        response->status = pb_resp.error_code();

        OpenResponse* open_resp = new OpenResponse();
        open_resp->st_ino = pb_resp.st_ino();
        open_resp->node_id = pb_resp.node_id();
        open_resp->st_dev = pb_resp.st_dev();
        open_resp->st_mode = pb_resp.st_mode();
        open_resp->st_nlink = pb_resp.st_nlink();
        open_resp->st_uid = pb_resp.st_uid();
        open_resp->st_gid = pb_resp.st_gid();
        open_resp->st_rdev = pb_resp.st_rdev();
        open_resp->st_size = pb_resp.st_size();
        open_resp->st_blksize = pb_resp.st_blksize();
        open_resp->st_blocks = pb_resp.st_blocks();
        open_resp->st_atim = pb_resp.st_atim();
        open_resp->st_mtim = pb_resp.st_mtim();
        open_resp->st_ctim = pb_resp.st_ctim();
        response->data = open_resp;
        return true;
    }

    if (operation == DFC_UNLINK) {
        falcon::meta_proto::UnlinkResponseData pb_resp;
        if (!pb_resp.ParseFromArray(p, pb_size)) {
            printf("[debug] [DeserializeResponseFromProtobuf] ERROR: Failed to parse UnlinkResponseData\n");
            fflush(stdout);
            return false;
        }
        response->status = pb_resp.error_code();

        UnlinkResponse* unlink_resp = new UnlinkResponse();
        unlink_resp->st_ino = pb_resp.st_ino();
        unlink_resp->st_size = pb_resp.st_size();
        unlink_resp->node_id = pb_resp.node_id();
        response->data = unlink_resp;
        return true;
    }

    if (operation == DFC_OPENDIR) {
        falcon::meta_proto::OpenDirResponseData pb_resp;
        if (!pb_resp.ParseFromArray(p, pb_size)) {
            printf("[debug] [DeserializeResponseFromProtobuf] ERROR: Failed to parse OpenDirResponseData\n");
            fflush(stdout);
            return false;
        }
        response->status = pb_resp.error_code();

        OpenDirResponse* opendir_resp = new OpenDirResponse();
        opendir_resp->st_ino = pb_resp.st_ino();
        response->data = opendir_resp;
        return true;
    }

    if (operation == DFC_READDIR) {
        falcon::meta_proto::ReadDirResponseData pb_resp;
        if (!pb_resp.ParseFromArray(p, pb_size)) {
            printf("[debug] [DeserializeResponseFromProtobuf] ERROR: Failed to parse ReadDirResponseData\n");
            fflush(stdout);
            return false;
        }
        response->status = pb_resp.error_code();

        ReadDirResponse* readdir_resp = new ReadDirResponse();
        readdir_resp->last_shard_index = pb_resp.last_shard_index();
        readdir_resp->last_file_name = pb_resp.last_file_name();
        for (const auto& entry_proto : pb_resp.entries()) {
            OneReadDirResponse entry;
            entry.file_name = entry_proto.file_name();
            entry.st_mode = entry_proto.st_mode();
            readdir_resp->result_list.push_back(entry);
        }
        response->data = readdir_resp;
        return true;
    }

    if (operation == DFC_GET_KV_META) {
        falcon::meta_proto::KvGetResponseData pb_resp;
        if (!pb_resp.ParseFromArray(p, pb_size)) {
            printf("[debug] [DeserializeResponseFromProtobuf] ERROR: Failed to parse KvGetResponseData\n");
            fflush(stdout);
            return false;
        }
        response->status = pb_resp.error_code();
        if (pb_resp.error_code() != 0) {
            response->data = nullptr;
            return true;
        }

        KvDataResponse* kv_resp = new KvDataResponse();
        kv_resp->kv_data.key = pb_resp.key();
        kv_resp->kv_data.valueLen = pb_resp.value_len();
        kv_resp->kv_data.sliceNum = pb_resp.slice_num();
        for (const auto& slice_proto : pb_resp.slices()) {
            FormDataSlice slice;
            slice.value_key = slice_proto.value_key();
            slice.location = slice_proto.location();
            slice.size = slice_proto.size();
            kv_resp->kv_data.dataSlices.push_back(slice);
        }
        response->data = kv_resp;
        return true;
    }

    if (operation == DFC_SLICE_GET) {
        falcon::meta_proto::SliceGetResponseData pb_resp;
        if (!pb_resp.ParseFromArray(p, pb_size)) {
            printf("[debug] [DeserializeResponseFromProtobuf] ERROR: Failed to parse SliceGetResponseData\n");
            fflush(stdout);
            return false;
        }
        response->status = pb_resp.error_code();

        SliceInfoResponse* slice_resp = new SliceInfoResponse();
        slice_resp->slicenum = pb_resp.slice_num();
        for (const auto& slice_proto : pb_resp.slices()) {
            slice_resp->inodeid.push_back(slice_proto.inode_id());
            slice_resp->chunkid.push_back(slice_proto.chunk_id());
            slice_resp->sliceid.push_back(slice_proto.slice_id());
            slice_resp->slicesize.push_back(slice_proto.slice_size());
            slice_resp->sliceoffset.push_back(slice_proto.slice_offset());
            slice_resp->slicelen.push_back(slice_proto.slice_len());
            slice_resp->sliceloc1.push_back(slice_proto.slice_loc1());
            slice_resp->sliceloc2.push_back(slice_proto.slice_loc2());
        }
        response->data = slice_resp;
        return true;
    }

    if (operation == DFC_FETCH_SLICE_ID) {
        falcon::meta_proto::FetchSliceIdResponseData pb_resp;
        if (!pb_resp.ParseFromArray(p, pb_size)) {
            printf("[debug] [DeserializeResponseFromProtobuf] ERROR: Failed to parse FetchSliceIdResponseData\n");
            fflush(stdout);
            return false;
        }
        response->status = pb_resp.error_code();

        SliceIdResponse* sliceid_resp = new SliceIdResponse();
        sliceid_resp->start = pb_resp.start();
        sliceid_resp->end = pb_resp.end();
        response->data = sliceid_resp;
        return true;
    }

    if (operation == DFC_PLAIN_COMMAND) {
        falcon::meta_proto::PlainCommandResponseData pb_resp;
        if (!pb_resp.ParseFromArray(p, pb_size)) {
            printf("[debug] [DeserializeResponseFromProtobuf] ERROR: Failed to parse PlainCommandResponseData\n");
            fflush(stdout);
            return false;
        }
        response->status = pb_resp.error_code();

        PlainCommandResponse* plain_resp = new PlainCommandResponse();
        plain_resp->row = pb_resp.row();
        plain_resp->col = pb_resp.col();
        for (const auto& item : pb_resp.data()) {
            plain_resp->data.push_back(item);
        }
        response->data = plain_resp;
        return true;
    }

    printf("[debug] [DeserializeResponseFromProtobuf] ERROR: Unsupported operation %d\n", operation);
    fflush(stdout);
    return false;
}

} // namespace meta_service
} // namespace falcon
