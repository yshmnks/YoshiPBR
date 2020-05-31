#pragma once

#include "ysJobSystem.h"

template<typename T>
struct JobData
{
    T* m_elements;
    ys_int32 m_count;
    void(*m_fcn)(T&);
    ysWorker* m_owner;
};

template<typename T>
void DivideAndConquerFor(ysJobSystem* sys, ysJob* job, void* dataPtr)
{
    JobData<T>* data = static_cast<JobData<T>*>(dataPtr);
    if (data->m_count < 256)
    {
        for (ys_int32 i = 0; i < data->m_count; ++i)
        {
            data->m_fcn(data->m_elements[i]);
        }
        ysJobSystemAllocation allocToFree;
        allocToFree.m_dataPtr = dataPtr;
        allocToFree.m_worker = data->m_owner;
        ysJobSystem_Free(&allocToFree, sizeof(JobData<T>));
        return;
    }

    ys_int32 countL = data->m_count / 2;
    ys_int32 countR = data->m_count - countL;

    ysJobSystemAllocation allocL = ysJobSystem_Allocate(sys, sizeof(JobData<T>));
    JobData<T>* dataL = static_cast<JobData<T>*>(allocL.m_dataPtr);
    dataL->m_elements = data->m_elements;
    dataL->m_count = countL;
    dataL->m_fcn = data->m_fcn;
    dataL->m_owner = allocL.m_worker;

    ysJobSystemAllocation allocR = ysJobSystem_Allocate(sys, sizeof(JobData<T>));
    JobData<T>* dataR = static_cast<JobData<T>*>(allocR.m_dataPtr);
    dataR->m_elements = data->m_elements + countL;
    dataR->m_count = countR;
    dataR->m_fcn = data->m_fcn;
    dataR->m_owner = allocR.m_worker;

    ysJobDef defL;
    defL.m_fcn = DivideAndConquerFor<T>;
    defL.m_fcnArg = dataL;
    defL.m_parentJob = job;

    ysJobDef defR;
    defR.m_fcn = DivideAndConquerFor<T>;
    defR.m_fcnArg = dataR;
    defR.m_parentJob = job;

    ysJob* jobL = ysJobSystem_CreateJob(sys, defL);
    ysJob* jobR = ysJobSystem_CreateJob(sys, defR);
    ysJobSystem_SubmitJob(sys, jobL);
    ysJobSystem_SubmitJob(sys, jobR);

    ysJobSystemAllocation allocToFree;
    allocToFree.m_dataPtr = dataPtr;
    allocToFree.m_worker = data->m_owner;
    ysJobSystem_Free(&allocToFree, sizeof(JobData<T>));
};

template<typename T>
void ysParallelFor(ysJobSystem* sys, T* elements, ys_int32 count, void(*fcn)(T& element))
{
    ysJobSystemAllocation alloc = ysJobSystem_Allocate(sys, sizeof(JobData<T>));
    JobData<T>* data = static_cast<JobData<T>*>(alloc.m_dataPtr);
    data->m_elements = elements;
    data->m_count = count;
    data->m_fcn = fcn;
    data->m_owner = alloc.m_worker;

    ysJobDef def;
    def.m_fcn = DivideAndConquerFor<T>;
    def.m_fcnArg = data;
    def.m_parentJob = nullptr;

    ysJob* job = ysJobSystem_CreateJob(sys, def);

    ysJobSystem_SubmitJob(sys, job);
    ysJobSystem_WaitOnJob(sys, job);
}