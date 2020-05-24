#include "ysWorker.h"
#include "ysWorkerManager.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void sBackgroundThreadFcn(void* workerPtr)
{
    ysWorker* worker = static_cast<ysWorker*>(workerPtr);
    worker->SleepUntilAlarm();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysWorker::Reset()
{
    m_mode = Mode::e_undefined;
    m_manager = nullptr;
    m_jobQueue.m_owner = this;
    m_jobQueue.SetToEmpty();
    m_thread.Reset();
    m_threadId = std::thread::id();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysWorker::CreateInForeground(ysWorkerManager* mgr)
{
    m_mode = Mode::e_foreground;
    m_manager = mgr;
    m_jobQueue.m_owner = this;
    m_jobQueue.SetToEmpty();
    m_thread.Reset();
    m_threadId = std::this_thread::get_id();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysWorker::CreateInBackground(ysWorkerManager* mgr)
{
    m_mode = Mode::e_background;
    m_manager = mgr;
    m_jobQueue.m_owner = this;
    m_jobQueue.SetToEmpty();
    m_thread.Create(sBackgroundThreadFcn, this);
    m_threadId = m_thread.GetID();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysWorker::Destroy()
{
    m_thread.Destroy();
    ysAssert(m_jobQueue.m_head == m_jobQueue.m_tail);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysWorker::Submit(ysJob* job)
{
    bool success = m_jobQueue.Push(job);
    if (success)
    {
        m_manager->m_alarmSemaphore.Signal(m_manager->m_workerCount);
    }
    else
    {
        job->m_fcn(job->m_fcnArg);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysWorker::Wait(ysJob* blockingJob)
{
    while (blockingJob->IsFinished() == false)
    {
        ysJob* job = GetJob();
        if (job != nullptr)
        {
            job->Execute();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysJob* ysWorker::GetJob()
{
    ysJob* ownJob = m_jobQueue.Pop();
    if (ownJob != nullptr)
    {
        return ownJob;
    }

    ysJob* stolenJob = m_manager->StealJobForPerpetrator(this);
    if (stolenJob == nullptr)
    {
        std::this_thread::yield();
        return nullptr;
    }
    return stolenJob;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysWorker::SleepUntilAlarm()
{
    // Wait until work exists
    m_manager->m_alarmSemaphore.Wait();

    while (true)
    {
        ysJob* job = GetJob();
        while (job != nullptr)
        {
            job->Execute();
            job = GetJob();
        }

        // Sleepy time. If another job gets pushed into any worker's queue we will wake up.
        m_manager->m_alarmSemaphore.Wait();
    }
}