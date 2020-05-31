#pragma once

#include "YoshiPBR/ysTypes.h"

struct ysJob;
struct ysJobSystem;
struct ysWorker;

typedef void ysJobFcn(ysJobSystem*, ysJob*, void* args);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysJobDef
{
    ysJobDef()
    {
        m_fcn = nullptr;
        m_fcnArg = nullptr;
        m_parentJob = nullptr;
    }

    ysJobFcn* m_fcn;
    void* m_fcnArg;
    ysJob* m_parentJob;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysJobSystemDef
{
    ysJobSystemDef()
    {
        m_workerCount = 1;
    }

    ys_int32 m_workerCount;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysJobSystemAllocation
{
    void* m_dataPtr;
    ysWorker* m_worker;
};

ysJobSystem* ysJobSystem_Create(const ysJobSystemDef&);
void ysJobSystem_Destroy(ysJobSystem*);

// No corresponding Destroy API is provided because a job should only be created if it should be run at some point. Once job execution
// finishes, the job system destroys it. It is generally unsafe to destroy jobs from outside of the job-system unless the job has not yet
// been sumbitted, but in that case, why Create it at all?
ysJob* ysJobSystem_CreateJob(ysJobSystem*, const ysJobDef&);

void ysJobSystem_SubmitJob(ysJobSystem*, ysJob*);
void ysJobSystem_WaitOnJob(ysJobSystem*, ysJob*);

ysJobSystemAllocation ysJobSystem_Allocate(ysJobSystem*, ys_int32 byteCount);
void ysJobSystem_Free(ysJobSystemAllocation*, ys_int32 byteCount);

// For debugging. It is advised to check that this returns TRUE before destroying the job system. FALSE is indicative of a leak.
bool ysJobSystem_AreResourcesEmptied(ysJobSystem*);