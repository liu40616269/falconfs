/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

#ifndef FALCON_POOLER_TASK_H
#define FALCON_POOLER_TASK_H

#include <brpc/server.h>
#include <vector>
#include "falcon_meta_rpc.pb.h"
#include "concurrentqueue/concurrentqueue.h"
#include "perf_counter/perf_stat.h"

namespace falcon::meta_proto
{

class AsyncMetaServiceJob {
  private:
    brpc::Controller *cntl;
    const MetaRequest *request;
    Empty *response;
    google::protobuf::Closure *done;

  public:
    LatencyTimer queueWaitTimer;
    uint64_t enqueueTimeNs;      // Time when job was enqueued (for inQueueLatency)
    uint64_t dequeueTimeNs;      // Time when job was dequeued (for workerWaitLatency)

    AsyncMetaServiceJob(brpc::Controller *cntl,
                        const MetaRequest *request,
                        Empty *response,
                        google::protobuf::Closure *done)
        : cntl(cntl),
          request(request),
          response(response),
          done(done),
          enqueueTimeNs(0),
          dequeueTimeNs(0)
    {
        queueWaitTimer.Start();
    }
    brpc::Controller *GetCntl() { return cntl; }
    const MetaRequest *GetRequest() { return request; }
    Empty *GetResponse() { return response; }
    void Done() { done->Run(); }
};

} // namespace falcon::meta_proto

class Task {
  public:
    bool isBatch;
    moodycamel::ConcurrentQueue<falcon::meta_proto::AsyncMetaServiceJob *> jobList;
    Task(int n) : jobList(n)
    {
        isBatch = false;
    }
    Task() { isBatch = false; }
};

class WorkerTask {
  public:
    bool isBatch;
    std::vector<falcon::meta_proto::AsyncMetaServiceJob *> jobList;
    WorkerTask() { isBatch = false; }
};

#endif
