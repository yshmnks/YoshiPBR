#pragma once

#include "ysWorker.h"
#include "YoshiPBR/ysThreading.h"

#define ysWORKERMANAGER_CAPACITY (64)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysWorkerManager
{
    void Reset();
    void Create(ys_int32 workerCount);
    void Destroy();

    ysWorker* GetForegroundWorker();
    ysWorker* GetWorkerForThisThread();
    ysJob* StealJobForPerpetrator(const ysWorker* perpetrator);

    ysWorker m_workers[ysWORKERMANAGER_CAPACITY];
    ys_int32 m_workerCount;
    ysSemaphore m_alarmSemaphore;
};