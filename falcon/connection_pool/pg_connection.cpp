/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

#include "connection_pool/pg_connection.h"

#include <iostream>
#include <sstream>

#include "falcon_meta_param_generated.h"
#include "falcon_meta_response_generated.h"
#include "falcon_meta_rpc.pb.h"
#include "connection_pool/falcon_meta_service.h"

extern "C" {
#include "connection_pool/connection_pool.h"
#include "utils/error_code.h"
#include "utils/utils_standalone.h"
}

PGConnection::PGConnection(PGConnectionPool *parent, const char *ip, const int port, const char *userName)
{
    this->parent = parent;

    working = true;
    taskToExec = nullptr;

    std::stringstream ss;
    ss << "hostaddr=" << ip << " port=" << port << " user=" << userName << " dbname=postgres";
    conn = PQconnectdb(ss.str().c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        throw std::runtime_error(std::string("pg connection error: ") + PQerrorMessage(conn));
    }
    PGresult *res = PQexec(conn, "SELECT falcon_prepare_commands();");
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        throw std::runtime_error(std::string("pg connection error: ") + PQresultErrorMessage(res));
    }

    SerializedDataInit(&replyBuilder, NULL, 0, 0, NULL);
    this->thread = std::thread(&PGConnection::BackgroundWorker, this);
}

void PGConnection::BackgroundWorker()
{
    while (working) {
        {
            std::unique_lock<std::mutex> lk(this->execMutex);
            cvExecing.wait(lk, [this]() -> bool { return this->taskToExec != nullptr || !working; });
            if (!working)
                break;
        }

        // 1. Reset status and check validity of input
        printf("[debug] [PGConnection] Processing task, jobList.size=%zu, isBatch=%d\n",
               taskToExec->jobList.size(), taskToExec->isBatch ? 1 : 0);
        fflush(stdout);

        PGresult *res;
        while ((res = PQgetResult(conn)) != NULL)
            PQclear(res);
        flatBufferBuilder.Clear();

        if (taskToExec->jobList.size() == 0)
            throw std::runtime_error("pgconnection: taskToExec is empty");

        // 2. Start processing
        FalconErrorCode errorCode = SUCCESS;
        FalconShmemAllocator *allocator = &FalconConnectionPoolShmemAllocator;
        if (taskToExec->isBatch) {
            printf("[debug] [PGConnection] BATCH operation path\n");
            fflush(stdout);
            // 2.1.1
            // if is batch operation,
            falcon::meta_proto::MetaServiceType serviceType = taskToExec->jobList[0]->GetRequest()->type(0);
            falcon::meta_proto::SerializationFormat format = taskToExec->jobList[0]->GetRequest()->format();

            uint32_t totalParamCount = 0;
            uint32_t totalParamSize = 0;
            for (size_t i = 0; i < taskToExec->jobList.size(); ++i) {
                size_t paramSize = taskToExec->jobList[i]->GetCntl()->request_attachment().size();
                // FlatBuffers format requires alignment, PROTOBUF does not
                if (format != falcon::meta_proto::SerializationFormat::PROTOBUF &&
                    (paramSize & SERIALIZED_DATA_ALIGNMENT_MASK) != 0)
                    throw std::runtime_error("param is corrupt."); // checked when init of job
                totalParamCount += taskToExec->jobList[i]->GetRequest()->type_size();
                totalParamSize += paramSize;
            }

            int64_t signature = FalconShmemAllocatorGetUniqueSignature(allocator);
            uint64_t totalParamShift = FalconShmemAllocatorMalloc(allocator, totalParamSize);
            if (totalParamShift == 0) {
                printf("Shmem of connection pool is exhausted, totalParamSize: %u. There may be "
                       "several reasons, 1) shmem size is too small, 2) allocate too much memory "
                       "once exceed FALCON_SHMEM_ALLOCATOR_MAX_SUPPORT_ALLOC_SIZE.",
                       totalParamSize);
                fflush(stdout);
                throw std::runtime_error("memory exceed limit.");
            }
            uint64_t p = totalParamShift;
            for (size_t i = 0; i < taskToExec->jobList.size(); ++i) {
                size_t paramSize = taskToExec->jobList[i]->GetCntl()->request_attachment().size();
                taskToExec->jobList[i]->GetCntl()->request_attachment().cutn(
                    FALCON_SHMEM_ALLOCATOR_GET_POINTER(allocator, p),
                    paramSize);
                p += paramSize;
            }
            FALCON_SHMEM_ALLOCATOR_SET_SIGNATURE(FALCON_SHMEM_ALLOCATOR_GET_POINTER(allocator, totalParamShift),
                                                 signature);

            // 2.1.2
            // barch operation can not be plain command
            uint64_t replyShift = 0;

            char command[128];

            if (format == falcon::meta_proto::SerializationFormat::PROTOBUF) {
                sprintf(command,
                        "select falcon_kv_meta_call_by_shmem_internal(%d, %u, %ld, %ld);",
                        serviceType,
                        totalParamCount,
                        (int64_t)totalParamShift,
                        signature);
            } else {
                sprintf(command,
                        "select falcon_meta_call_by_serialized_shmem_internal(%d, %u, %ld, %ld);",
                        serviceType,
                        totalParamCount,
                        (int64_t)totalParamShift,
                        signature);
            }

            int sendQuerySucceed = PQsendQuery(conn, command);
            if (sendQuerySucceed != 1)
                throw std::runtime_error(PQerrorMessage(conn));

            PGresult *res = NULL;
            res = PQgetResult(conn);
            if (res == NULL)
                throw std::runtime_error(PQerrorMessage(conn));
            // param is useless now
            FalconShmemAllocatorFree(allocator, totalParamShift);
            if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                char *totalErrorMsg = PQresultErrorMessage(res);
                const char *validErrorMsg = NULL;
                errorCode = FalconErrorMsgAnalyse(totalErrorMsg, &validErrorMsg);
                if (errorCode == SUCCESS)
                    errorCode = PROGRAM_ERROR;
            }

            // 2.1.3 Process result
            if (errorCode != SUCCESS) {
                if (format == falcon::meta_proto::SerializationFormat::PROTOBUF) {
                    // PROTOBUF format error response
                    falcon::meta_proto::SimpleResponseData pb_resp;
                    pb_resp.set_error_code(errorCode);
                    std::string pbData;
                    pb_resp.SerializeToString(&pbData);

                    // Build response with length prefix for each job
                    for (size_t i = 0; i < taskToExec->jobList.size(); ++i) {
                        brpc::Controller *cntl = taskToExec->jobList[i]->GetCntl();
                        int count = taskToExec->jobList[i]->GetRequest()->type_size();

                        // Each request in the job needs an error response
                        std::vector<char> jobData;
                        for (int j = 0; j < count; ++j) {
                            uint32_t len = pbData.size();
                            jobData.insert(jobData.end(), (char*)&len, (char*)&len + sizeof(uint32_t));
                            jobData.insert(jobData.end(), pbData.begin(), pbData.end());
                        }

                        char *data = (char *)malloc(jobData.size());
                        memcpy(data, jobData.data(), jobData.size());
                        cntl->response_attachment().append_user_data(data, jobData.size(), NULL);
                        taskToExec->jobList[i]->Done();
                    }
                } else {
                    // FlatBuffers format error response
                    SerializedDataClear(&replyBuilder);
                    flatBufferBuilder.Clear();
                    auto metaResponse = falcon::meta_fbs::CreateMetaResponse(flatBufferBuilder, errorCode);
                    flatBufferBuilder.Finish(metaResponse);
                    char *buf = SerializedDataApplyForSegment(&replyBuilder, flatBufferBuilder.GetSize());
                    memcpy(buf, flatBufferBuilder.GetBufferPointer(), flatBufferBuilder.GetSize());

                    for (size_t i = 0; i < taskToExec->jobList.size(); ++i) {
                        brpc::Controller *cntl = taskToExec->jobList[i]->GetCntl();
                        char *data = (char *)malloc(replyBuilder.size);
                        memcpy(data, replyBuilder.buffer, replyBuilder.size);
                        cntl->response_attachment().append_user_data(data, replyBuilder.size, NULL);
                        taskToExec->jobList[i]->Done();
                    }
                }
            } else {
                if (PQntuples(res) != 1 || PQnfields(res) != 1) {
                    throw std::runtime_error("returned reply is corrupt.");
                }
                replyShift = (uint64_t)StringToInt64(PQgetvalue(res, 0, 0));
                if (replyShift != 0) {
                    char *replyBuffer = FALCON_SHMEM_ALLOCATOR_GET_POINTER(allocator, replyShift);
                    uint64_t replyBufferSize = FALCON_SHMEM_ALLOCATOR_POINTER_GET_SIZE(replyBuffer);

                    if (format == falcon::meta_proto::SerializationFormat::PROTOBUF) {
                        size_t offset = 0;
                        for (size_t i = 0; i < taskToExec->jobList.size(); ++i) {
                            brpc::Controller *cntl = taskToExec->jobList[i]->GetCntl();
                            int count = taskToExec->jobList[i]->GetRequest()->type_size();

                            std::vector<char> jobData;
                            for (int j = 0; j < count; ++j) {
                                if (offset + sizeof(uint32_t) > replyBufferSize)
                                    throw std::runtime_error("PROTOBUF response buffer overflow reading length prefix");

                                // Read length prefix
                                uint32_t responseLen = *(uint32_t*)(replyBuffer + offset);
                                offset += sizeof(uint32_t);

                                if (offset + responseLen > replyBufferSize)
                                    throw std::runtime_error("PROTOBUF response buffer overflow reading data");

                                // Append length prefix + response data for protobuf deserialization
                                uint32_t len = responseLen;
                                jobData.insert(jobData.end(), (char*)&len, (char*)&len + sizeof(uint32_t));
                                jobData.insert(jobData.end(), replyBuffer + offset, replyBuffer + offset + responseLen);
                                offset += responseLen;
                            }

                            char *data = (char *)malloc(jobData.size());
                            memcpy(data, jobData.data(), jobData.size());
                            cntl->response_attachment().append_user_data(data, jobData.size(), NULL);
                            taskToExec->jobList[i]->Done();
                        }
                    } else {
                        // FlatBuffers 格式：使用 SerializedData 解析
                        SerializedData replyData;
                        if (!SerializedDataInit(&replyData, replyBuffer, replyBufferSize, replyBufferSize, NULL))
                            throw std::runtime_error("reply data is corrupt.");

                        uint32_t p = 0;
                        for (size_t i = 0; i < taskToExec->jobList.size(); ++i) {
                            brpc::Controller *cntl = taskToExec->jobList[i]->GetCntl();

                            int count = taskToExec->jobList[i]->GetRequest()->type_size();
                            uint32_t size = SerializedDataNextSeveralItemSize(&replyData, p, count);
                            if (size == (sd_size_t)-1)
                                throw std::runtime_error("response is corrupt.");
                            char *data = (char *)malloc(size);
                            memcpy(data, replyBuffer + p, size);
                            cntl->response_attachment().append_user_data(data, size, NULL);

                            taskToExec->jobList[i]->Done();
                            p += size;
                        }
                    }
                    FalconShmemAllocatorFree(allocator, replyShift);
                } else {
                    for (size_t i = 0; i < taskToExec->jobList.size(); ++i) {
                        taskToExec->jobList[i]->Done();
                    }
                }
            }

            PQclear(res);
        } else {
            printf("[debug] [PGConnection] NON-BATCH operation path\n");
            fflush(stdout);

            if (taskToExec->jobList.size() != 1)
                throw std::runtime_error("pgconnection: jobList.size() must be 1 for non-batch operation");

            // 2.2.1 Copy data into shmem
            falcon::meta_proto::AsyncMetaServiceJob *job = taskToExec->jobList[0];
            size_t paramSize = job->GetCntl()->request_attachment().size();
            printf("[debug] [PGConnection] Copying request to shmem, paramSize=%zu\n", paramSize);
            fflush(stdout);
            uint64_t paramShift = FalconShmemAllocatorMalloc(allocator, paramSize);
            if (paramShift == 0) {
                printf("Shmem of connection pool is exhausted, paramSize: %zu. There may be "
                       "several reasons, 1) shmem size is too small, 2) allocate too much memory "
                       "once exceed FALCON_SHMEM_ALLOCATOR_MAX_SUPPORT_ALLOC_SIZE.",
                       paramSize);
                fflush(stdout);
                throw std::runtime_error("memory exceed limit.");
            }
            char *paramBuffer = FALCON_SHMEM_ALLOCATOR_GET_POINTER(allocator, paramShift);
            job->GetCntl()->request_attachment().cutn(paramBuffer, paramSize);

            // 2.2.2
            std::stringstream toSendCommand;
            std::vector<bool> isPlainCommand;
            std::vector<int64_t> signatureList;
            falcon::meta_proto::SerializationFormat format = job->GetRequest()->format();

            SerializedData requestData;
            if (format == falcon::meta_proto::SerializationFormat::FLATBUFFER) {
                if (!SerializedDataInit(&requestData, paramBuffer, paramSize, paramSize, NULL))
                    throw std::runtime_error("request attachment is corrupt.");
            }

            int i = 0;
            uint64_t currentParamSegment = 0;
            while (i < job->GetRequest()->type_size()) {
                falcon::meta_proto::MetaServiceType serviceType = job->GetRequest()->type(i);
                int j = i + 1;
                if (serviceType != falcon::meta_proto::MetaServiceType::PLAIN_COMMAND) {
                    while (j < job->GetRequest()->type_size() && job->GetRequest()->type(j) == serviceType)
                        ++j;
                }
                int currentParamSegmentCount = j - i;

                uint32_t currentParamSegmentSize;
                if (format == falcon::meta_proto::SerializationFormat::PROTOBUF) {
                    currentParamSegmentSize = 0;
                    size_t offset = currentParamSegment;
                    for (int k = i; k < j; ++k) {
                        if (offset + sizeof(uint32_t) > paramSize)
                            throw std::runtime_error("PROTOBUF request buffer overflow");
                        uint32_t pbLen = *(uint32_t*)(paramBuffer + offset);
                        offset += sizeof(uint32_t) + pbLen;
                    }
                    currentParamSegmentSize = offset - currentParamSegment;
                } else {
                    currentParamSegmentSize =
                        SerializedDataNextSeveralItemSize(&requestData, currentParamSegment, j - i);
                }

                if (serviceType == falcon::meta_proto::MetaServiceType::PLAIN_COMMAND) {
                    std::string command;

                    if (format == falcon::meta_proto::SerializationFormat::PROTOBUF) {
                        char *p = paramBuffer + currentParamSegment;
                        uint32_t pbLen = *(uint32_t*)p;
                        p += sizeof(uint32_t);
                        falcon::meta_proto::PlainCommandRequestData pb_req;
                        if (!pb_req.ParseFromArray(p, pbLen))
                            throw std::runtime_error("Failed to parse PROTOBUF PlainCommandRequest");
                        command = pb_req.command();
                    } else {
                        char *buf = paramBuffer + currentParamSegment + SERIALIZED_DATA_ALIGNMENT;
                        int size = currentParamSegmentSize - SERIALIZED_DATA_ALIGNMENT;
                        flatbuffers::Verifier verifier((uint8_t *)buf, size);
                        if (!verifier.VerifyBuffer<falcon::meta_fbs::MetaParam>())
                            throw std::runtime_error("request param is corrupt. 1");
                        const falcon::meta_fbs::MetaParam *param = falcon::meta_fbs::GetMetaParam(buf);
                        if (param->param_type() != falcon::meta_fbs::AnyMetaParam::AnyMetaParam_PlainCommandParam)
                            throw std::runtime_error("request param is corrupt. 2");
                        command = param->param_as_PlainCommandParam()->command()->c_str();
                    }

                    toSendCommand << command;

                    isPlainCommand.push_back(true);
                    signatureList.push_back(0);
                } else {
                    signatureList.push_back(FalconShmemAllocatorGetUniqueSignature(allocator));

                    if (format == falcon::meta_proto::SerializationFormat::PROTOBUF) {
                        toSendCommand << "select falcon_kv_meta_call_by_shmem_internal(" << serviceType << ", "
                                      << currentParamSegmentCount << ", " << paramShift + currentParamSegment << ", "
                                      << signatureList.back() << ");";
                    } else {
                        toSendCommand << "select falcon_meta_call_by_serialized_shmem_internal(" << serviceType << ", "
                                      << currentParamSegmentCount << ", " << paramShift + currentParamSegment << ", "
                                      << signatureList.back() << ");";
                    }

                    isPlainCommand.push_back(false);
                }

                currentParamSegment += currentParamSegmentSize;
                i = j;
            }

            // 2.2.3
            printf("[debug] [PGConnection] Executing SQL: %s\n", toSendCommand.str().c_str());
            fflush(stdout);

            PQsendQuery(conn, toSendCommand.str().c_str());
            std::vector<PGresult *> result;
            PGresult *res = NULL;
            while ((res = PQgetResult(conn)) != NULL)
                result.push_back(res);
            FalconShmemAllocatorFree(allocator, paramShift);

            printf("[debug] [PGConnection] SQL executed, result.size=%zu\n", result.size());
            fflush(stdout);
            if (result.size() != isPlainCommand.size()) {
                throw std::runtime_error(
                    "reply count cannot match request. maybe there is a request containing several plain commands.");
            }
            // 2.2.4
            if (format == falcon::meta_proto::SerializationFormat::PROTOBUF) {
                printf("[debug] [PGConnection] Processing PROTOBUF format response\n");
                fflush(stdout);

                std::vector<char> binaryReplyBuffer;

                for (size_t i = 0; i < result.size(); ++i) {
                    res = result[i];
                    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                        char *totalErrorMsg = PQresultErrorMessage(res);
                        const char *validErrorMsg = NULL;
                        FalconErrorCode errorCode = FalconErrorMsgAnalyse(totalErrorMsg, &validErrorMsg);
                        if (errorCode == SUCCESS)
                            errorCode = PROGRAM_ERROR;

                        falcon::meta_proto::SimpleResponseData pb_resp;
                        pb_resp.set_error_code(errorCode);
                        std::string pbData;
                        pb_resp.SerializeToString(&pbData);
                        uint32_t pbLen = pbData.size();
                        binaryReplyBuffer.insert(binaryReplyBuffer.end(), (char*)&pbLen, (char*)&pbLen + sizeof(pbLen));
                        binaryReplyBuffer.insert(binaryReplyBuffer.end(), pbData.begin(), pbData.end());

                        printf("[debug] [PGConnection] PROTOBUF format error response: errorCode=%d, msg=%s\n",
                               errorCode, validErrorMsg ? validErrorMsg : totalErrorMsg);
                        fflush(stdout);
                    } else if (isPlainCommand[i]) {
                        falcon::meta_proto::PlainCommandResponseData pb_resp;
                        pb_resp.set_error_code(SUCCESS);
                        pb_resp.set_row(PQntuples(res));
                        pb_resp.set_col(PQnfields(res));
                        for (int r = 0; r < PQntuples(res); ++r) {
                            for (int c = 0; c < PQnfields(res); ++c) {
                                pb_resp.add_data(PQgetvalue(res, r, c));
                            }
                        }
                        std::string pbData;
                        pb_resp.SerializeToString(&pbData);
                        uint32_t pbLen = pbData.size();
                        binaryReplyBuffer.insert(binaryReplyBuffer.end(), (char*)&pbLen, (char*)&pbLen + sizeof(pbLen));
                        binaryReplyBuffer.insert(binaryReplyBuffer.end(), pbData.begin(), pbData.end());
                    } else {
                        int64_t signature = signatureList[i];
                        if (PQntuples(res) != 1 || PQnfields(res) != 1)
                            throw std::runtime_error("returned reply is corrupt in non-batch operation. 1");
                        uint64_t replyShift = (uint64_t)StringToInt64(PQgetvalue(res, 0, 0));
                        printf("[debug] [PGConnection] Reading response from shmem, replyShift=%lu, signature=%ld\n",
                               replyShift, signature);
                        fflush(stdout);

                        char *replyBuffer = FALCON_SHMEM_ALLOCATOR_GET_POINTER(allocator, replyShift);
                        if (FALCON_SHMEM_ALLOCATOR_GET_SIGNATURE(replyBuffer) != signature)
                            throw std::runtime_error("returned reply is corrupt in non-batch operation. 2");
                        uint64_t replyBufferSize = FALCON_SHMEM_ALLOCATOR_POINTER_GET_SIZE(replyBuffer);

                        printf("[debug] [PGConnection] replyBufferSize=%lu, stripping length prefixes\n", replyBufferSize);
                        fflush(stdout);

                        size_t offset = 0;
                        while (offset < replyBufferSize) {
                            if (offset + sizeof(uint32_t) > replyBufferSize)
                                throw std::runtime_error("PROTOBUF response buffer overflow reading length prefix");

                            uint32_t responseLen = *(uint32_t*)(replyBuffer + offset);

                            printf("[debug] [PGConnection] Response chunk: length_prefix=%u, offset=%zu\n", responseLen, offset);
                            fflush(stdout);

                            if (offset + sizeof(uint32_t) + responseLen > replyBufferSize)
                                throw std::runtime_error("PROTOBUF response buffer overflow reading data");

                            // Include length prefix + protobuf data (client expects this format)
                            binaryReplyBuffer.insert(binaryReplyBuffer.end(),
                                                     replyBuffer + offset,
                                                     replyBuffer + offset + sizeof(uint32_t) + responseLen);
                            offset += sizeof(uint32_t) + responseLen;
                        }

                        FalconShmemAllocatorFree(allocator, replyShift);
                    }
                }

                printf("[debug] [PGConnection] Sending PROTOBUF response, total_size=%zu\n", binaryReplyBuffer.size());
                fflush(stdout);

                char *data = (char *)malloc(binaryReplyBuffer.size());
                memcpy(data, binaryReplyBuffer.data(), binaryReplyBuffer.size());
                job->GetCntl()->response_attachment().append_user_data(data, binaryReplyBuffer.size(), NULL);
                job->Done();

                printf("[debug] [PGConnection] Response sent successfully\n");
                fflush(stdout);
            } else {
                SerializedData replyData;
                SerializedDataInit(&replyData, NULL, 0, 0, NULL);

                for (size_t i = 0; i < result.size(); ++i) {
                    res = result[i];
                    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                        char *totalErrorMsg = PQresultErrorMessage(res);
                        const char *validErrorMsg = NULL;
                        FalconErrorCode errorCode = FalconErrorMsgAnalyse(totalErrorMsg, &validErrorMsg);
                        if (errorCode == SUCCESS)
                            errorCode = PROGRAM_ERROR;

                        flatBufferBuilder.Clear();
                        auto metaResponse = falcon::meta_fbs::CreateMetaResponse(flatBufferBuilder, errorCode);
                        flatBufferBuilder.Finish(metaResponse);

                        char *buf = SerializedDataApplyForSegment(&replyData, flatBufferBuilder.GetSize());
                        memcpy(buf, flatBufferBuilder.GetBufferPointer(), flatBufferBuilder.GetSize());
                    } else if (isPlainCommand[i]) {
                        flatBufferBuilder.Clear();
                        std::vector<flatbuffers::Offset<flatbuffers::String>> plainCommandResponseData;
                        int row = PQntuples(res);
                        int col = PQnfields(res);
                        for (int i = 0; i < row; ++i)
                            for (int j = 0; j < col; ++j)
                                plainCommandResponseData.push_back(flatBufferBuilder.CreateString(PQgetvalue(res, i, j)));
                        auto plainCommandResponse = falcon::meta_fbs::CreatePlainCommandResponse(
                            flatBufferBuilder,
                            row,
                            col,
                            flatBufferBuilder.CreateVector(plainCommandResponseData));
                        auto metaResponse = falcon::meta_fbs::CreateMetaResponse(
                            flatBufferBuilder,
                            SUCCESS,
                            falcon::meta_fbs::AnyMetaResponse::AnyMetaResponse_PlainCommandResponse,
                            plainCommandResponse.Union());
                        flatBufferBuilder.Finish(metaResponse);

                        char *buf = SerializedDataApplyForSegment(&replyData, flatBufferBuilder.GetSize());
                        memcpy(buf, flatBufferBuilder.GetBufferPointer(), flatBufferBuilder.GetSize());
                    } else {
                        int64_t signature = signatureList[i];
                        if (PQntuples(res) != 1 || PQnfields(res) != 1)
                            throw std::runtime_error("returned reply is corrupt in non-batch operation. 1");
                        uint64_t replyShift = (uint64_t)StringToInt64(PQgetvalue(res, 0, 0));
                        char *replyBuffer = FALCON_SHMEM_ALLOCATOR_GET_POINTER(allocator, replyShift);
                        if (FALCON_SHMEM_ALLOCATOR_GET_SIGNATURE(replyBuffer) != signature)
                            throw std::runtime_error("returned reply is corrupt in non-batch operation. 2");
                        uint64_t replyBufferSize = FALCON_SHMEM_ALLOCATOR_POINTER_GET_SIZE(replyBuffer);

                        // FlatBuffers format: use SerializedData
                        SerializedData oneReply;
                        if (!SerializedDataInit(&oneReply, replyBuffer, replyBufferSize, replyBufferSize, NULL))
                            throw std::runtime_error("reply data is corrupt.");
                        SerializedDataAppend(&replyData, &oneReply);

                        FalconShmemAllocatorFree(allocator, replyShift);
                    }
                }

                // Send FlatBuffers data with SerializedData wrapper
                job->GetCntl()->response_attachment().append_user_data(replyData.buffer, replyData.size, NULL);
                job->Done();
            }

            for (size_t i = 0; i < result.size(); ++i)
                PQclear(res);
        }

        // TBD
        //
        //

        this->parent->ReaddWorkingPGConnection(this);

        for (size_t i = 0; i < taskToExec->jobList.size(); ++i)
            delete taskToExec->jobList[i];
        delete this->taskToExec;
        {
            std::unique_lock<std::mutex> lk(this->execMutex);
            this->taskToExec = nullptr;
        }
        cvExecing.notify_one();
    }
}

void PGConnection::Exec(Task *taskToExec)
{
    {
        std::unique_lock<std::mutex> lk(this->execMutex);
        cvExecing.wait(lk, [this]() -> bool { return this->taskToExec == nullptr; });
        this->taskToExec = taskToExec;
    }
    cvExecing.notify_one();
}

void PGConnection::Stop()
{
    working = false;
    cvExecing.notify_one();
}

PGConnection::~PGConnection()
{
    Stop();
    thread.join();
    if (conn) {
        PQfinish(conn);
        conn = nullptr;
    }
    SerializedDataDestroy(&replyBuilder);
}
