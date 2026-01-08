/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

#include "postgres.h"
#include "storage/shmem.h"
#include "storage/lwlock.h"
#include "perf_counter/falcon_perf_latency_shmem.h"

/* Global shared memory pointer */
FalconPerfLatencyShmem* g_FalconPerfLatencyShmem = NULL;

/* LWLock tranche ID for performance counters */
static int falcon_perf_lwlock_tranche_id = 0;

/*
 * Calculate shared memory size required for performance counters
 */
Size
FalconPerfLatencyShmemSize(void)
{
    return MAXALIGN(sizeof(FalconPerfLatencyShmem));
}

/*
 * Initialize shared memory for performance counters
 */
void FalconPerfLatencyShmemInit(void)
{
    bool found;
    Size size = FalconPerfLatencyShmemSize();

    g_FalconPerfLatencyShmem = (FalconPerfLatencyShmem*)
        ShmemInitStruct("FalconPerfLatency", size, &found);

    if (!found)
    {
        /* First time initialization - zero out all data */
        memset(g_FalconPerfLatencyShmem, 0, size);

        /* Initialize all LatencyData structures */
        /* Connection Pool Layer */
        LatencyDataInit(&g_FalconPerfLatencyShmem->queueWaitLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->enqueueDelayLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->inQueueLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->connWaitLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->workerWaitLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->shmemCopyLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->pgExecTotalLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->resultProcLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->totalRequestLatency);

        /* Metadata Handling Layer */
        LatencyDataInit(&g_FalconPerfLatencyShmem->pathVerifyLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->pathParseLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->tableOpenLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->indexScanLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->tableInsertLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->tableUpdateLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->tableDeleteLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->commitLatency);

        /* Directory Table Operations */
        LatencyDataInit(&g_FalconPerfLatencyShmem->dirInsertLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->dirSearchLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->dirDeleteLatency);

        /* Remote Call */
        LatencyDataInit(&g_FalconPerfLatencyShmem->remoteCallLatency);

        /* KV/Slice Layer */
        LatencyDataInit(&g_FalconPerfLatencyShmem->kvPutLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->kvGetLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->kvDelLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->slicePutLatency);
        LatencyDataInit(&g_FalconPerfLatencyShmem->sliceGetLatency);

        /* Initialize LWLocks */
        falcon_perf_lwlock_tranche_id = LWLockNewTrancheId();
        LWLockRegisterTranche(falcon_perf_lwlock_tranche_id, "falcon_perf_latency");

        /* Initialize locks for all LatencyData structures */
        LWLockInitialize(&g_FalconPerfLatencyShmem->queueWaitLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->enqueueDelayLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->inQueueLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->connWaitLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->workerWaitLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->shmemCopyLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->pgExecTotalLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->resultProcLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->totalRequestLatency.lock,
                        falcon_perf_lwlock_tranche_id);

        LWLockInitialize(&g_FalconPerfLatencyShmem->pathVerifyLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->pathParseLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->tableOpenLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->indexScanLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->tableInsertLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->tableUpdateLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->tableDeleteLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->commitLatency.lock,
                        falcon_perf_lwlock_tranche_id);

        LWLockInitialize(&g_FalconPerfLatencyShmem->dirInsertLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->dirSearchLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->dirDeleteLatency.lock,
                        falcon_perf_lwlock_tranche_id);

        LWLockInitialize(&g_FalconPerfLatencyShmem->remoteCallLatency.lock,
                        falcon_perf_lwlock_tranche_id);

        LWLockInitialize(&g_FalconPerfLatencyShmem->kvPutLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->kvGetLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->kvDelLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->slicePutLatency.lock,
                        falcon_perf_lwlock_tranche_id);
        LWLockInitialize(&g_FalconPerfLatencyShmem->sliceGetLatency.lock,
                        falcon_perf_lwlock_tranche_id);
    }
}

/*
 * Report latency to shared memory (PostgreSQL backend only - uses LWLock)
 */
void ReportLatencyToShmem(LatencyData* data, uint64 latencyNs)
{
    if (data == NULL)
        return;

    LWLockAcquire(&data->lock, LW_EXCLUSIVE);
    data->sum_ns += latencyNs;
    data->count++;
    if (latencyNs < data->min_ns)
        data->min_ns = latencyNs;
    if (latencyNs > data->max_ns)
        data->max_ns = latencyNs;
    LWLockRelease(&data->lock);
}

/*
 * Report latency to shared memory using atomic operations.
 * Safe to call from non-PostgreSQL threads (C++ std::thread).
 */
void ReportLatencyToShmemAtomic(LatencyData* data, uint64 latencyNs)
{
    if (data == NULL)
        return;

    /* Atomic add for sum and count */
    __atomic_add_fetch(&data->sum_ns, latencyNs, __ATOMIC_RELAXED);
    __atomic_add_fetch(&data->count, 1, __ATOMIC_RELAXED);

    /* CAS loop for min */
    uint64 old_min = __atomic_load_n(&data->min_ns, __ATOMIC_RELAXED);
    while (latencyNs < old_min) {
        if (__atomic_compare_exchange_n(&data->min_ns, &old_min, latencyNs,
                                        false, __ATOMIC_RELAXED, __ATOMIC_RELAXED))
            break;
    }

    /* CAS loop for max */
    uint64 old_max = __atomic_load_n(&data->max_ns, __ATOMIC_RELAXED);
    while (latencyNs > old_max) {
        if (__atomic_compare_exchange_n(&data->max_ns, &old_max, latencyNs,
                                        false, __ATOMIC_RELAXED, __ATOMIC_RELAXED))
            break;
    }
}

/*
 * Getter functions for LatencyData pointers
 * These allow C++ code to access specific latency counters without
 * including PostgreSQL headers (which conflict with glog/brpc).
 */
LatencyData* GetTotalRequestLatencyData(void)
{
    return g_FalconPerfLatencyShmem ? &g_FalconPerfLatencyShmem->totalRequestLatency : NULL;
}

LatencyData* GetQueueWaitLatencyData(void)
{
    return g_FalconPerfLatencyShmem ? &g_FalconPerfLatencyShmem->queueWaitLatency : NULL;
}

LatencyData* GetShmemCopyLatencyData(void)
{
    return g_FalconPerfLatencyShmem ? &g_FalconPerfLatencyShmem->shmemCopyLatency : NULL;
}

LatencyData* GetPgExecTotalLatencyData(void)
{
    return g_FalconPerfLatencyShmem ? &g_FalconPerfLatencyShmem->pgExecTotalLatency : NULL;
}

LatencyData* GetResultProcLatencyData(void)
{
    return g_FalconPerfLatencyShmem ? &g_FalconPerfLatencyShmem->resultProcLatency : NULL;
}

LatencyData* GetEnqueueDelayLatencyData(void)
{
    return g_FalconPerfLatencyShmem ? &g_FalconPerfLatencyShmem->enqueueDelayLatency : NULL;
}

LatencyData* GetInQueueLatencyData(void)
{
    return g_FalconPerfLatencyShmem ? &g_FalconPerfLatencyShmem->inQueueLatency : NULL;
}

LatencyData* GetConnWaitLatencyData(void)
{
    return g_FalconPerfLatencyShmem ? &g_FalconPerfLatencyShmem->connWaitLatency : NULL;
}

LatencyData* GetWorkerWaitLatencyData(void)
{
    return g_FalconPerfLatencyShmem ? &g_FalconPerfLatencyShmem->workerWaitLatency : NULL;
}
