/* Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MulanPSL-2.0
 */

#include "control/control_flag.h"

#include "fmgr.h"
#include "port/atomics.h"
#include "storage/lwlock.h"
#include "storage/shmem.h"

#include "utils/shmem_control.h"

static ShmemControlData *FalconControlShmemControl;
static pg_atomic_uint32 *FalconBackgroundServiceStarted;

PG_FUNCTION_INFO_V1(falcon_start_background_service);
Datum falcon_start_background_service(PG_FUNCTION_ARGS)
{
    pg_atomic_exchange_u32(FalconBackgroundServiceStarted, 1);
    PG_RETURN_INT16(0);
}

bool CheckFalconBackgroundServiceStarted() { return pg_atomic_read_u32(FalconBackgroundServiceStarted) != 0; }

size_t FalconControlShmemsize() { return sizeof(ShmemControlData) + sizeof(pg_atomic_uint32); }
void FalconControlShmemInit()
{
    bool initialized = false;

    FalconControlShmemControl = ShmemInitStruct("Falcon Control", FalconControlShmemsize(), &initialized);
    FalconBackgroundServiceStarted = (pg_atomic_uint32 *)(FalconControlShmemControl + 1);
    if (!initialized) {
        FalconControlShmemControl->trancheId = LWLockNewTrancheId();
        FalconControlShmemControl->lockTrancheName = "Falcon Control";
        LWLockRegisterTranche(FalconControlShmemControl->trancheId, FalconControlShmemControl->lockTrancheName);
        LWLockInitialize(&FalconControlShmemControl->lock, FalconControlShmemControl->trancheId);

        pg_atomic_init_u32(FalconBackgroundServiceStarted, 0);
    }
}

static bool FalconInAbortProgress = false;
bool FalconIsInAbortProgress(void) { return FalconInAbortProgress; }
void FalconEnterAbortProgress(void) { FalconInAbortProgress = true; }
void FalconQuitAbortProgress(void) { FalconInAbortProgress = false; }
