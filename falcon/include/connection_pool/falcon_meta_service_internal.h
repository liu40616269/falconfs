/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

#ifndef FALCON_META_SERVICE_INTERNAL_H
#define FALCON_META_SERVICE_INTERNAL_H

#include "connection_pool/falcon_meta_service.h"
#include <brpc/controller.h>
#include "falcon_meta_rpc.pb.h"

namespace falcon {
namespace meta_service {

/**
 * Falcon 元数据服务序列化工具类
 *
 * 提供 PROTOBUF 格式的序列化/反序列化功能
 * 依赖 BRPC 和 Protobuf
 */
class FalconMetaServiceSerializer {
public:
    /**
     * 将 Falcon 元数据请求序列化为 Protobuf 格式
     *
     * @param request: Falcon 元数据服务请求
     * @param proto_request: Protobuf 请求对象（输出）
     * @param attachment: BRPC 附件（输出）
     * @return: true 表示成功，false 表示失败
     *
     * 请求格式规范：
     * [长度: 4字节] + [protobuf序列化数据: 长度字节]
     */
    static bool SerializeRequestToProtobuf(
        const FalconMetaServiceRequest& request,
        falcon::meta_proto::MetaRequest* proto_request,
        butil::IOBuf* attachment);

    /**
     * 从 PROTOBUF 格式反序列化 Falcon 元数据响应
     *
     * @param attachment: BRPC 附件（包含 protobuf 序列化的响应数据）
     * @param response: Falcon 元数据服务响应（输出）
     * @param operation: 操作类型
     * @return: true 表示成功，false 表示失败
     *
     * PROTOBUF 响应格式：
     * [length: 4 bytes] + [protobuf_serialized_data: length bytes]
     *
     * 每个操作类型对应不同的 protobuf message:
     * - SimpleResponseData: 只有 error_code
     * - CreateResponseData/StatResponseData/OpenResponseData: 包含文件属性
     * - UnlinkResponseData/OpenDirResponseData: 包含部分属性
     * - ReadDirResponseData: 包含目录项列表
     * - KvGetResponseData: 包含 KV 元数据
     * - SliceGetResponseData: 包含 Slice 元数据
     * - FetchSliceIdResponseData: 包含 SliceID 范围
     */
    static bool DeserializeResponseFromProtobuf(
        const butil::IOBuf& attachment,
        FalconMetaServiceResponse* response,
        FalconMetaOperationType operation);
};

} // namespace meta_service
} // namespace falcon

#endif // FALCON_META_SERVICE_INTERNAL_H
