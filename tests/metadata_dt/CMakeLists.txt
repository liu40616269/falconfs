include(CTest)

link_directories(${POSTGRES_LIB_DIR})

add_executable(FalconMetadataDT
    ${PROJECT_SOURCE_DIR}/tests/metadata_dt/metadata_dt_client.cpp
    ${PROJECT_SOURCE_DIR}/tests/metadata_dt/test_metadata_dt.cpp
    ${common_src}
)

target_include_directories(FalconMetadataDT PUBLIC
    ${PROJECT_SOURCE_DIR}/falcon_store/src/include
    ${PROJECT_SOURCE_DIR}/falcon_client/src/include
    ${PROJECT_SOURCE_DIR}/common/src/include
    ${PROJECT_SOURCE_DIR}/falcon/include
    ${POSTGRES_INCLUDE_DIR}
)

target_link_libraries(FalconMetadataDT
    FalconStore
    FalconClient
    zookeeper_mt
    glog
    jsoncpp
    gtest
    pq
    ${BRPC_LIBRARIES}
    ${DYNAMIC_LIB}
)

add_test(
    NAME FalconMetadataDT
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run_metadata_dt.sh
)

set_tests_properties(FalconMetadataDT PROPERTIES
    LABELS "dt;metadata;e2e"
    TIMEOUT 300
)
