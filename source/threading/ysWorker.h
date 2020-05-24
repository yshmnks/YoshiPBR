#pragma once

#include "ysJobPool.h"
#include "ysJobQueue.h"
#include "YoshiPBR/ysThreading.h"

struct ysWorkerManager;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysWorker
{
    enum struct Mode
    {
        e_undefined,
        e_foreground,
        e_background,
    };

    void Reset();
    void CreateInForeground(ysWorkerManager*);
    void CreateInBackground(ysWorkerManager*);
    void Destroy();

    void Submit(ysJob* job);
    void Wait(ysJob* job);

    ysJob* GetJob();

    // This is the loop run by BACKGROUND workers.
    // The FOREGROUND thread also has a corresponding worker, but never sleeps. Once it has finished submitting jobs, it "waits" and joins
    // in on the work juggling until all workers' job queues (including its own) are emptied, at which point the foreground thread resumes.
    void SleepUntilAlarm();

    ysWorkerManager* m_manager;
    ysJobQueue m_jobQueue;
    ysThread m_thread;
    std::thread::id m_threadId;

    Mode m_mode;
};