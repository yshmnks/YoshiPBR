#pragma once

#include "ysJob.h"

#define ysJOBPOOL_CAPACITY (16)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysJobPool
{
    void Reset();
    void Create();

    ysJob* AllocateJob();
    void ClearJobs();

    ysJob m_jobs[ysJOBPOOL_CAPACITY];
    ys_int32 m_jobCount;
};