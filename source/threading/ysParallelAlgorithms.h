#pragma once

#include "ysJobSystem.h"

template<typename T, typename SharedDataT>
struct JobData
{
    T* m_elements;
    ys_int32 m_count;
    SharedDataT* m_sharedData;
    ys_int32 m_jobSplittingThreshold;
    void(*m_fcn)(T&, SharedDataT* sharedData);
    ysWorker* m_owner;
};

template<typename T, typename SharedDataT>
void DivideAndConquerFor(ysJobSystem* sys, ysJob* job, void* dataPtr)
{
    JobData<T, SharedDataT>* data = static_cast<JobData<T, SharedDataT>*>(dataPtr);
    if (data->m_count < data->m_jobSplittingThreshold)
    {
        for (ys_int32 i = 0; i < data->m_count; ++i)
        {
            data->m_fcn(data->m_elements[i], data->m_sharedData);
        }
        ysJobSystemAllocation allocToFree;
        allocToFree.m_dataPtr = dataPtr;
        allocToFree.m_worker = data->m_owner;
        ysJobSystem_Free(&allocToFree, sizeof(JobData<T, SharedDataT>));
        return;
    }

    ys_int32 countL = data->m_count / 2;
    ys_int32 countR = data->m_count - countL;

    ysJobSystemAllocation allocL = ysJobSystem_Allocate(sys, sizeof(JobData<T, SharedDataT>));
    JobData<T, SharedDataT>* dataL = static_cast<JobData<T, SharedDataT>*>(allocL.m_dataPtr);
    dataL->m_elements = data->m_elements;
    dataL->m_count = countL;
    dataL->m_sharedData = data->m_sharedData;
    dataL->m_jobSplittingThreshold = data->m_jobSplittingThreshold;
    dataL->m_fcn = data->m_fcn;
    dataL->m_owner = allocL.m_worker;

    ysJobSystemAllocation allocR = ysJobSystem_Allocate(sys, sizeof(JobData<T, SharedDataT>));
    JobData<T, SharedDataT>* dataR = static_cast<JobData<T, SharedDataT>*>(allocR.m_dataPtr);
    dataR->m_elements = data->m_elements + countL;
    dataR->m_count = countR;
    dataR->m_sharedData = data->m_sharedData;
    dataR->m_jobSplittingThreshold = data->m_jobSplittingThreshold;
    dataR->m_fcn = data->m_fcn;
    dataR->m_owner = allocR.m_worker;

    ysJobDef defL;
    defL.m_fcn = DivideAndConquerFor<T, SharedDataT>;
    defL.m_fcnArg = dataL;
    defL.m_parentJob = job;

    ysJobDef defR;
    defR.m_fcn = DivideAndConquerFor<T, SharedDataT>;
    defR.m_fcnArg = dataR;
    defR.m_parentJob = job;

    ysJob* jobL = ysJobSystem_CreateJob(sys, defL);
    ysJob* jobR = ysJobSystem_CreateJob(sys, defR);
    ysJobSystem_SubmitJob(sys, jobL);
    ysJobSystem_SubmitJob(sys, jobR);

    ysJobSystemAllocation allocToFree;
    allocToFree.m_dataPtr = dataPtr;
    allocToFree.m_worker = data->m_owner;
    ysJobSystem_Free(&allocToFree, sizeof(JobData<T, SharedDataT>));
};

template<typename T, typename SharedDataT>
void ysParallelFor(ysJobSystem* sys,
    T* elements, ys_int32 count, SharedDataT* sharedData,
    ys_int32 jobSplittingThreshold,
    void(*fcn)(T& element, SharedDataT* sharedData))
{
    ysJobSystemAllocation alloc = ysJobSystem_Allocate(sys, sizeof(JobData<T, SharedDataT>));
    JobData<T, SharedDataT>* data = static_cast<JobData<T, SharedDataT>*>(alloc.m_dataPtr);
    data->m_elements = elements;
    data->m_count = count;
    data->m_sharedData = sharedData;
    data->m_jobSplittingThreshold = jobSplittingThreshold;
    data->m_fcn = fcn;
    data->m_owner = alloc.m_worker;

    ysJobDef def;
    def.m_fcn = DivideAndConquerFor<T, SharedDataT>;
    def.m_fcnArg = data;
    def.m_parentJob = nullptr;

    ysJob* job = ysJobSystem_CreateJob(sys, def);

    ysJobSystem_SubmitJob(sys, job);
    ysJobSystem_WaitOnJob(sys, job);
}