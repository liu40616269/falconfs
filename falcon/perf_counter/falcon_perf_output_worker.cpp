/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

extern "C" {
#include "postgres.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "utils/elog.h"
#include "perf_counter/falcon_perf_latency_shmem.h"
}

/* Signal handling */
static volatile sig_atomic_t got_sigterm = false;

static void falcon_perf_output_sigterm(SIGNAL_ARGS)
{
    int save_errno = errno;
    got_sigterm = true;
    SetLatch(MyLatch);
    errno = save_errno;
}

/* Output a single LatencyData field and reset it. Returns true if data was output. */
static bool OutputAndResetLatencyData(const char* name, LatencyData* data)
{
    if (data == NULL)
        return false;

    LWLockAcquire(&data->lock, LW_EXCLUSIVE);

    if (data->count == 0) {
        LWLockRelease(&data->lock);
        return false;
    }

    /* Read values */
    uint64 sumUs = data->sum_ns / 1000;
    uint64 count = data->count;
    double avgUs = (double)data->sum_ns / data->count / 1000.0;
    uint64 minUs = data->min_ns / 1000;
    uint64 maxUs = data->max_ns / 1000;

    /* Reset for next interval */
    data->sum_ns = 0;
    data->count = 0;
    data->min_ns = UINT64_MAX;
    data->max_ns = 0;

    LWLockRelease(&data->lock);

    ereport(LOG, (errmsg("        %s: avg/min/max/sum=%.0f/%llu/%llu/%lluus cnt=%llu",
                         name, avgUs,
                         (unsigned long long)minUs,
                         (unsigned long long)maxUs,
                         (unsigned long long)sumUs,
                         (unsigned long long)count)));
    return true;
}

/* Check if LatencyData has data (without locking, for quick check) */
static bool HasLatencyData(LatencyData* data)
{
    return data != NULL && data->count > 0;
}

/* Output all performance statistics and reset counters */
static void OutputAllStats(void)
{
    FalconPerfLatencyShmem* perf = g_FalconPerfLatencyShmem;
    if (perf == NULL)
        return;

    /* Quick check if there's any data to output */
    bool hasData = HasLatencyData(&perf->queueWaitLatency) ||
                   HasLatencyData(&perf->enqueueDelayLatency) ||
                   HasLatencyData(&perf->inQueueLatency) ||
                   HasLatencyData(&perf->connWaitLatency) ||
                   HasLatencyData(&perf->workerWaitLatency) ||
                   HasLatencyData(&perf->shmemCopyLatency) ||
                   HasLatencyData(&perf->pgExecTotalLatency) ||
                   HasLatencyData(&perf->resultProcLatency) ||
                   HasLatencyData(&perf->totalRequestLatency) ||
                   HasLatencyData(&perf->pathVerifyLatency) ||
                   HasLatencyData(&perf->pathParseLatency) ||
                   HasLatencyData(&perf->tableOpenLatency) ||
                   HasLatencyData(&perf->indexScanLatency) ||
                   HasLatencyData(&perf->tableInsertLatency) ||
                   HasLatencyData(&perf->tableUpdateLatency) ||
                   HasLatencyData(&perf->tableDeleteLatency) ||
                   HasLatencyData(&perf->commitLatency) ||
                   HasLatencyData(&perf->dirInsertLatency) ||
                   HasLatencyData(&perf->dirSearchLatency) ||
                   HasLatencyData(&perf->dirDeleteLatency) ||
                   HasLatencyData(&perf->remoteCallLatency) ||
                   HasLatencyData(&perf->kvPutLatency) ||
                   HasLatencyData(&perf->kvGetLatency) ||
                   HasLatencyData(&perf->kvDelLatency) ||
                   HasLatencyData(&perf->slicePutLatency) ||
                   HasLatencyData(&perf->sliceGetLatency);

    if (!hasData)
        return;

    /* Print header */
    ereport(LOG, (errmsg("========== Falcon Perf (interval) ==========")));

    /* Connection Pool Layer */
    OutputAndResetLatencyData("queueWait",       &perf->queueWaitLatency);
    OutputAndResetLatencyData("enqueueDelay",    &perf->enqueueDelayLatency);
    OutputAndResetLatencyData("inQueue",         &perf->inQueueLatency);
    OutputAndResetLatencyData("connWait",        &perf->connWaitLatency);
    OutputAndResetLatencyData("workerWait",      &perf->workerWaitLatency);
    OutputAndResetLatencyData("shmemCopy",       &perf->shmemCopyLatency);
    OutputAndResetLatencyData("pgExecTotal",     &perf->pgExecTotalLatency);
    OutputAndResetLatencyData("resultProc",      &perf->resultProcLatency);
    OutputAndResetLatencyData("totalRequest",    &perf->totalRequestLatency);

    /* Metadata Handling Layer */
    OutputAndResetLatencyData("pathVerify",      &perf->pathVerifyLatency);
    OutputAndResetLatencyData("pathParse",       &perf->pathParseLatency);
    OutputAndResetLatencyData("tableOpen",       &perf->tableOpenLatency);
    OutputAndResetLatencyData("indexScan",       &perf->indexScanLatency);
    OutputAndResetLatencyData("tableInsert",     &perf->tableInsertLatency);
    OutputAndResetLatencyData("tableUpdate",     &perf->tableUpdateLatency);
    OutputAndResetLatencyData("tableDelete",     &perf->tableDeleteLatency);
    OutputAndResetLatencyData("commit",          &perf->commitLatency);

    /* Directory Table Operations */
    OutputAndResetLatencyData("dirInsert",       &perf->dirInsertLatency);
    OutputAndResetLatencyData("dirSearch",       &perf->dirSearchLatency);
    OutputAndResetLatencyData("dirDelete",       &perf->dirDeleteLatency);

    /* Remote Call */
    OutputAndResetLatencyData("remoteCall",      &perf->remoteCallLatency);

    /* KV/Slice Layer */
    OutputAndResetLatencyData("kvPut",           &perf->kvPutLatency);
    OutputAndResetLatencyData("kvGet",           &perf->kvGetLatency);
    OutputAndResetLatencyData("kvDel",           &perf->kvDelLatency);
    OutputAndResetLatencyData("slicePut",        &perf->slicePutLatency);
    OutputAndResetLatencyData("sliceGet",        &perf->sliceGetLatency);
}

/* Background worker main function */
extern "C" void FalconPerfOutputWorkerMain(Datum main_arg)
{
    /* Signal handling setup */
    pqsignal(SIGTERM, falcon_perf_output_sigterm);
    BackgroundWorkerUnblockSignals();

    /* Check shared memory */
    if (g_FalconPerfLatencyShmem == NULL) {
        ereport(ERROR, (errmsg("Falcon perf shared memory not initialized")));
        proc_exit(1);
    }

    ereport(LOG, (errmsg("Falcon performance output worker started")));

    /* Main loop: output statistics every 60 seconds */
    int outputIntervalSec = 60;
    while (!got_sigterm) {
        int rc = WaitLatch(MyLatch,
                          WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
                          outputIntervalSec * 1000L,
                          PG_WAIT_EXTENSION);
        ResetLatch(MyLatch);

        if (rc & WL_POSTMASTER_DEATH) {
            proc_exit(1);
        }

        if (got_sigterm) {
            break;
        }

        OutputAllStats();
    }

    ereport(LOG, (errmsg("Falcon performance output worker stopped")));
    proc_exit(0);
}

/* Register background worker */
extern "C" void RegisterFalconPerfOutputWorker(void)
{
    BackgroundWorker worker;

    memset(&worker, 0, sizeof(BackgroundWorker));
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
    worker.bgw_start_time = BgWorkerStart_PostmasterStart;
    worker.bgw_restart_time = BGW_NEVER_RESTART;
    worker.bgw_notify_pid = 0;
    snprintf(worker.bgw_name, BGW_MAXLEN, "falcon_perf_output");
    snprintf(worker.bgw_type, BGW_MAXLEN, "falcon_perf_output");
    worker.bgw_main_arg = (Datum) 0;
    sprintf(worker.bgw_library_name, "falcon");
    sprintf(worker.bgw_function_name, "FalconPerfOutputWorkerMain");

    RegisterBackgroundWorker(&worker);
}
