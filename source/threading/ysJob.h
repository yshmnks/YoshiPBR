#pragma once

#include "YoshiPBR/ysTypes.h"
#include <atomic>

struct ysJob;
struct ysWorkerManager;

typedef void ysJobFcn(ysJob& job, void* args);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysJobDef
{
    ysJobDef()
    {
        m_workerMgr = nullptr;
        m_fcn = nullptr;
        m_fcnArg = nullptr;
        m_parentJob = nullptr;
    }

    ysWorkerManager* m_workerMgr;
    ysJobFcn* m_fcn;
    void* m_fcnArg;
    ysJob* m_parentJob;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysJob
{
    void Reset();
    void Create(const ysJobDef&);

    void Execute();
    bool IsFinished() const;

    void __Finish();

    ysWorkerManager* m_workerMgr;
    ysJobFcn* m_fcn;
    void* m_fcnArg;
    ysJob* m_parent;
    std::atomic<ys_int32> m_unfinishedJobCount;

    /////////////
    // PADDING //
    /////////////
    struct PayloadFormat
    {
        ysWorkerManager* a;
        ysJobFcn* b;
        void* c;
        ysJob* d;
        std::atomic<ys_int32> e;
    };
    static constexpr std::size_t PAYLOAD_SIZE = sizeof(PayloadFormat);
    static constexpr std::size_t FALSE_SHARING_SIZE = std::hardware_destructive_interference_size;
    ys_uint8 _PAD[FALSE_SHARING_SIZE - PAYLOAD_SIZE];
};