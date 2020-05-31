#include "YoshiPBR/ysUnitTests.h"
#include "YoshiPBR/ysMemoryPool.h"
#include "threading/ysParallelAlgorithms.h"
#include <thread>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysUnitTest_Memory()
{
    struct Allocation
    {
        void* m_ptr;
        ys_int32 m_size;
    };

    const ys_uint8 k_defaultByteValue = 0x08;
    const ys_int32 k_maxAllocCount = 8888;
    const ys_int32 k_iterCount = 8; // Each iteration is a batch of allocations followed by a batch of frees

    ys_uint8 cmpBuffer[ysMemoryPool::e_maxChunkSize];
    ysMemSet(cmpBuffer, k_defaultByteValue, sizeof(cmpBuffer));

    ysMemoryPool memPool;
    memPool.Create();

    Allocation* allocations = static_cast<Allocation*>(ysMalloc(sizeof(Allocation) * k_maxAllocCount));
    ys_int32 allocCount = 0;

    for (ys_int32 iter = 0; iter < k_iterCount; ++iter)
    {
        for (; allocCount < k_maxAllocCount; ++allocCount)
        {
            ys_int32 size = 1 + (std::rand() % ysMemoryPool::e_maxChunkSize);
            void* ptr = memPool.Allocate(size);
            ysMemSet(ptr, k_defaultByteValue, size);
            allocations[allocCount].m_ptr = ptr;
            allocations[allocCount].m_size = size;
        }

        ys_int32 freeCount = std::rand() % allocCount;
        for (ys_int32 i = 0; i < freeCount; ++i)
        {
            ys_int32 idxToFree = std::rand() % allocCount;
            memPool.Free(allocations[idxToFree].m_ptr, allocations[idxToFree].m_size);
            allocations[idxToFree] = allocations[--allocCount];
        }

        memPool.ValidateBlocks();
        memPool.ValidateFreeLists();
        for (ys_int32 i = 0; i < allocCount; ++i)
        {
            ys_int32 cmpRes = ysMemCmp(allocations[i].m_ptr, cmpBuffer, allocations[i].m_size);
            ysAssert(cmpRes == 0);
            memPool.ValidateAllocation(allocations[i].m_ptr, allocations[i].m_size);
        }
    }

    ysFree(allocations);
    memPool.Destroy();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void sParallelForUnitTestFcn(ys_int32& a)
{
    a++;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysUnitTest_JobSystem()
{
    ys_int32 elemCount = 88888888;
    ys_int32* elems = static_cast<ys_int32*>(ysMalloc(sizeof(ys_int32) * elemCount));
    ysMemSet(elems, 0, sizeof(ys_int32) * elemCount);
    unsigned int threadCount = std::thread::hardware_concurrency();
    ysJobSystemDef jobSysDef;
    jobSysDef.m_workerCount = threadCount;
    ysJobSystem* jobSys = ysJobSystem_Create(jobSysDef);
    ysParallelFor(jobSys, elems, elemCount, sParallelForUnitTestFcn);
    ysParallelFor(jobSys, elems, elemCount / 2, sParallelForUnitTestFcn);
    for (ys_int32 i = 0; i < elemCount / 2; ++i)
    {
        ysAssert(elems[i] == 2);
    }
    for (ys_int32 i = elemCount / 2; i < elemCount; ++i)
    {
        ysAssert(elems[i] == 1);
    }
    ysSafeFree(elems);
    bool safeForShutdown = ysJobSystem_AreResourcesEmptied(jobSys);
    ysAssert(safeForShutdown);
    ysJobSystem_Destroy(jobSys);
}