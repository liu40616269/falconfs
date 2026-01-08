/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

#ifndef FALCON_PERF_LATENCY_SHMEM_H
#define FALCON_PERF_LATENCY_SHMEM_H

#include "postgres.h"
#include "storage/lwlock.h"

#ifdef __cplusplus
extern "C" {
#endif

/* POD structure for latency statistics in shared memory */
typedef struct LatencyData {
    LWLock lock;
    uint64 sum_ns;      /* Total latency in nanoseconds */
    uint64 count;       /* Number of samples */
    uint64 min_ns;      /* Minimum latency */
    uint64 max_ns;      /* Maximum latency */
} LatencyData;

/* Shared memory structure for latency statistics */
typedef struct FalconPerfLatencyShmem {
    /* ===== Connection Pool Layer (pg_connection.cpp) ===== */
    LatencyData queueWaitLatency;       /* Total queue wait time */
    LatencyData enqueueDelayLatency;    /* Enqueue delay */
    LatencyData inQueueLatency;         /* Time in queue */
    LatencyData connWaitLatency;        /* Wait for connection */
    LatencyData workerWaitLatency;      /* Wait for worker */
    LatencyData shmemCopyLatency;       /* Shared memory copy */
    LatencyData pgExecTotalLatency;     /* PG execution total (black box) */
    LatencyData resultProcLatency;      /* Result processing */
    LatencyData totalRequestLatency;    /* End-to-end request latency */

    /* ===== Metadata Handling Layer (meta_handle.c) ===== */
    LatencyData pathVerifyLatency;      /* Path verification */
    LatencyData pathParseLatency;       /* Path parsing (directory table) */
    LatencyData tableOpenLatency;       /* Open table and index */
    LatencyData indexScanLatency;       /* Index scan */
    LatencyData tableInsertLatency;     /* Table insert */
    LatencyData tableUpdateLatency;     /* Table update */
    LatencyData tableDeleteLatency;     /* Table delete */
    LatencyData commitLatency;          /* Transaction commit */

    /* ===== Directory Table Operations (dir_path_hash.c) ===== */
    LatencyData dirInsertLatency;       /* Directory table insert */
    LatencyData dirSearchLatency;       /* Directory table search */
    LatencyData dirDeleteLatency;       /* Directory table delete */

    /* ===== Remote Call (CN -> DN) ===== */
    LatencyData remoteCallLatency;      /* Remote call to DN (send + wait) */

    /* ===== KV/Slice Layer ===== */
    LatencyData kvPutLatency;           /* KV PUT total */
    LatencyData kvGetLatency;           /* KV GET total */
    LatencyData kvDelLatency;           /* KV DEL total */
    LatencyData slicePutLatency;        /* Slice PUT */
    LatencyData sliceGetLatency;        /* Slice GET */

} FalconPerfLatencyShmem;

/* Global shared memory pointer */
extern FalconPerfLatencyShmem* g_FalconPerfLatencyShmem;

/* Shared memory functions */
extern Size FalconPerfLatencyShmemSize(void);
extern void FalconPerfLatencyShmemInit(void);

/* Background worker registration (defined in falcon_perf_output_worker.cpp) */
extern void RegisterFalconPerfOutputWorker(void);

/* Initialize a single LatencyData structure */
static inline void LatencyDataInit(LatencyData *data)
{
    data->sum_ns = 0;
    data->count = 0;
    data->min_ns = UINT64_MAX;
    data->max_ns = 0;
}

/* Report latency to shared memory (PostgreSQL backend only - uses LWLock) */
extern void ReportLatencyToShmem(LatencyData* data, uint64 latencyNs);

/* Report latency using atomic operations (safe for C++ std::thread) */
extern void ReportLatencyToShmemAtomic(LatencyData* data, uint64 latencyNs);

/* Getter functions for LatencyData pointers (to avoid exposing struct layout) */
extern LatencyData* GetTotalRequestLatencyData(void);
extern LatencyData* GetQueueWaitLatencyData(void);
extern LatencyData* GetShmemCopyLatencyData(void);
extern LatencyData* GetPgExecTotalLatencyData(void);
extern LatencyData* GetResultProcLatencyData(void);
extern LatencyData* GetEnqueueDelayLatencyData(void);
extern LatencyData* GetInQueueLatencyData(void);
extern LatencyData* GetConnWaitLatencyData(void);
extern LatencyData* GetWorkerWaitLatencyData(void);

#ifdef __cplusplus
}
#endif

#endif /* FALCON_PERF_LATENCY_SHMEM_H */
