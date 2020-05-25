#pragma once

#include "ysJobSystem.h"

template<typename T>
struct JobData
{
    T* m_elements;
    ys_int32 m_count;
    void(*m_fcn)(T&);
};

template<typename T>
void DivideAndConquerFor(ysWorkerManager* mgr, ysJob* job, void* dataPtr)
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

    JobData<T>* dataL = ysNew JobData<T>;
    dataL->m_elements = data->m_elements;
    dataL->m_count = countL;
    dataL->m_fcn = data->m_fcn;

    JobData<T>* dataR = ysNew JobData<T>;
    dataR->m_elements = data->m_elements + countL;
    dataR->m_count = countR;
    dataR->m_fcn = data->m_fcn;

    ysJobDef defL;
    defL.m_fcn = DivideAndConquerFor<T>;
    defL.m_fcnArg = dataL;
    defL.m_parentJob = job;

    ysJobDef defR;
    defR.m_fcn = DivideAndConquerFor<T>;
    defR.m_fcnArg = dataR;
    defR.m_parentJob = job;

    ysJob* jobL = ysWorkerManager_CreateJob(mgr, defL);
    ysJob* jobR = ysWorkerManager_CreateJob(mgr, defR);
    ysWorkerManager_SubmitJob(mgr, jobL);
    ysWorkerManager_SubmitJob(mgr, jobR);
};

template<typename T>
void ysParallelFor(ysWorkerManager* mgr, T* elements, ys_int32 count, void(*fcn)(T& element))
{
    JobData<T>* data = ysNew JobData<T>;
    data->m_elements = elements;
    data->m_count = count;
    data->m_fcn = fcn;

    ysJobDef def;
    def.m_fcn = DivideAndConquerFor<T>;
    def.m_fcnArg = data;
    def.m_parentJob = nullptr;

    ysJob* job = ysWorkerManager_CreateJob(mgr, def);

    ysWorkerManager_SubmitJob(mgr, job);
    ysWorkerManager_WaitOnJob(mgr, job);
}