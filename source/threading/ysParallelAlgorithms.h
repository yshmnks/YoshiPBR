#pragma once

#include "ysWorkerManager.h"

template<typename T>
struct JobData
{
    ysWorkerManager* m_workerMgr;
    ysJob* m_job;
    T* m_elements;
    ys_int32 m_count;
    void(*m_fcn)(T&);
};

template<typename T>
void DivideAndConquerFor(void* dataPtr)
{
    JobData<T>* data = static_cast<JobData<T>*>(dataPtr);
    if (data->m_count < 256)
    {
        for (ys_int32 i = 0; i < data->m_count; ++i)
        {
            data->m_fcn(data->m_elements[i]);
        }
        return;
    }

    ys_int32 countL = data->m_count / 2;
    ys_int32 countR = data->m_count - countL;

    ysJob jobL;
    ysJob jobR;

    JobData<T> dataL;
    {
        dataL.m_workerMgr = data->m_workerMgr;
        dataL.m_job = &jobL;
        dataL.m_elements = data->m_elements;
        dataL.m_count = countL;
        dataL.m_fcn = data->m_fcn;
    }

    JobData<T> dataR;
    {
        dataR.m_workerMgr = data->m_workerMgr;
        dataR.m_job = &jobR;
        dataR.m_elements = data->m_elements + countL;
        dataR.m_count = countR;
        dataR.m_fcn = data->m_fcn;
    }

    ysWorker* workerForThisThread = data->m_workerMgr->GetWorkerForThisThread();

    jobL.Create(DivideAndConquerFor<T>, &dataL, data->m_job);
    jobR.Create(DivideAndConquerFor<T>, &dataR, data->m_job);

    workerForThisThread->Submit(&jobL);
    workerForThisThread->Submit(&jobR);
};

template<typename T>
void ysParallelFor(ysWorkerManager* mgr, T* elements, ys_int32 count, void(*fcn)(T& element))
{
    ysJob rootJob;

    JobData<T> rootData;
    rootData.m_job = &rootJob;
    rootData.m_workerMgr = mgr;
    rootData.m_elements = elements;
    rootData.m_count = count;
    rootData.m_fcn = fcn;

    ysWorker* worker = mgr->GetForegroundWorker();
    rootJob.Create(DivideAndConquerFor<T>, &rootData, nullptr);

    worker->Submit(&rootJob);
    worker->Wait(&rootJob);
}