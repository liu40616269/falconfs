# 生成 Protobuf 代码
# protobuf_generate_cpp(PROTO_SRC PROTO_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/remote_connection_def/proto/falcon_meta_rpc.proto)
# protobuf_generate_cpp(BRPC_SRC BRPC_HEADER ${PROJECT_SOURCE_DIR}/falcon_store/src/brpc/brpc_io.proto)

# 将生成的代码目录添加到包含路径
# include_directories(${CMAKE_CURRENT_BINARY_DIR})
# 生成 falcon_meta_rpc.proto 的代码
add_custom_command(
    OUTPUT ${PROTO_SRC} ${PROTO_HEADER}
    COMMAND protoc --cpp_out=${CMAKE_BINARY_DIR} --proto_path=${CMAKE_SOURCE_DIR}/remote_connection_def/proto falcon_meta_rpc.proto
    DEPENDS ${CMAKE_SOURCE_DIR}/remote_connection_def/proto/falcon_meta_rpc.proto
    COMMENT "Generating Protobuf files for falcon_meta_rpc.proto"
)

# 生成 brpc_io.proto 的代码
add_custom_command(
    OUTPUT ${BRPC_SRC} ${BRPC_HEADER}
    COMMAND protoc --cpp_out=${CMAKE_BINARY_DIR} --proto_path=${CMAKE_SOURCE_DIR}/remote_connection_def/proto brpc_io.proto
    DEPENDS ${CMAKE_SOURCE_DIR}/remote_connection_def/proto/brpc_io.proto
    COMMENT "Generating Protobuf files for brpc_io.proto"
)
# 定义一个自定义目标，用于管理 Protobuf 生成的代码
add_custom_target(ProtobufGenerated ALL
    DEPENDS ${PROTO_SRC} ${PROTO_HEADER} ${BRPC_SRC} ${BRPC_HEADER}
    COMMENT "Generating Protobuf and BRPC source files"
)
