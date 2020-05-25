#include "YoshiPBR/ysMemoryPool.h"

ys_int32 ysMemoryPool::s_chunkSizes[e_chunkSizeCount];
ys_int32 ysMemoryPool::s_objSizeToChunkSizeIdx[1 + e_maxChunkSize];
bool ysMemoryPool::s_staticDataInitialized = false;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysMemoryPool::Reset()
{
    m_blocks.Create();
    for (ys_int32 i = 0; i < e_chunkSizeCount; ++i)
    {
        m_freeLists[i] = nullptr;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysMemoryPool::Create()
{
    if (s_staticDataInitialized == false)
    {
        for (ys_int32 i = 0; i < e_chunkSizeCount; ++i)
        {
            s_chunkSizes[i] = (i + 1) * e_chunkSizeIncrement;
        }

        s_objSizeToChunkSizeIdx[0] = ys_nullIndex;
        ys_int32 chunkSizeIdx = 0;
        for (ys_int32 objSize = 1; objSize <= e_maxChunkSize; ++objSize)
        {
            if (objSize > s_chunkSizes[chunkSizeIdx])
            {
                chunkSizeIdx++;
                ysAssert(chunkSizeIdx < e_chunkSizeCount);
            }
            s_objSizeToChunkSizeIdx[objSize] = chunkSizeIdx;
        }
        ysAssert(chunkSizeIdx == e_chunkSizeCount - 1);
    }

    m_blocks.Create();
    m_blocks.SetCapacity(64);
    m_blocks.SetCount(0);

    for (ys_int32 i = 0; i < e_chunkSizeCount; ++i)
    {
        m_freeLists[i] = nullptr;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysMemoryPool::Destroy()
{
    for (ys_int32 i = 0; i < m_blocks.GetCount(); ++i)
    {
        ysFree(m_blocks[i].m_chunks);
    }
    m_blocks.Destroy();

    for (ys_int32 i = 0; i < e_chunkSizeCount; ++i)
    {
        m_freeLists[i] = nullptr;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void* ysMemoryPool::Allocate(ys_int32 size)
{
    ysAssert(0 <= size && size <= e_maxChunkSize);
    if (size <= 0 || e_maxChunkSize < size)
    {
        return nullptr;
    }
    ys_int32 chunkSizeIdx = s_objSizeToChunkSizeIdx[size];
    ys_int32 chunkSize = s_chunkSizes[chunkSizeIdx];
    Chunk* freeChunk = m_freeLists[chunkSizeIdx];
    if (freeChunk != nullptr)
    {
        m_freeLists[chunkSizeIdx] = freeChunk->m_nextInFreeList;
        return freeChunk;
    }
    Block* block = m_blocks.Allocate();
    block->m_chunkSize = chunkSize;
    block->m_chunkCount = e_blockSize / chunkSize;
    ysAssert(block->m_chunkCount > 0);
    block->m_chunks = static_cast<Chunk*>(ysMalloc(block->m_chunkSize * block->m_chunkCount));
    ys_uint8* chunkBytes = reinterpret_cast<ys_uint8*>(block->m_chunks);
    m_freeLists[chunkSizeIdx] = reinterpret_cast<Chunk*>(chunkBytes + chunkSize);
    for (ys_int32 i = 1; i < block->m_chunkCount - 1; ++i)
    {
        Chunk* chunk0 = reinterpret_cast<Chunk*>(chunkBytes + (i + 0) * chunkSize);
        Chunk* chunk1 = reinterpret_cast<Chunk*>(chunkBytes + (i + 1) * chunkSize);
        chunk0->m_nextInFreeList = chunk1;
    }
    Chunk* lastChunk = reinterpret_cast<Chunk*>(chunkBytes + (block->m_chunkCount - 1) * chunkSize);
    lastChunk->m_nextInFreeList = nullptr;
    return block->m_chunks + 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysMemoryPool::Free(void* obj, ys_int32 size)
{
#if ysDEBUG_BUILD
    ValidateAllocation(obj, size);
#endif
    ysAssert(0 <= size && size <= e_maxChunkSize);
    if (size <= 0 || e_maxChunkSize < size)
    {
        return;
    }
    ys_int32 chunkSizeIdx = s_objSizeToChunkSizeIdx[size];
    Chunk* chunk = static_cast<Chunk*>(obj);
    chunk->m_nextInFreeList = m_freeLists[chunkSizeIdx];
    m_freeLists[chunkSizeIdx] = chunk;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysMemoryPool::ValidateAllocation(void* obj, ys_int32 size) const
{
    ysAssert(0 <= size && size <= e_maxChunkSize);
    ys_int32 chunkSizeIdx = s_objSizeToChunkSizeIdx[size];
    ys_int32 chunkSize = s_chunkSizes[chunkSizeIdx];
    ys_uint8* bytePtr = static_cast<ys_uint8*>(obj);

    bool found = false;
    for (ys_int32 i = 0; i < m_blocks.GetCount(); ++i)
    {
        const Block& block = m_blocks[i];
        ys_int32 byteOffset = ys_int32(bytePtr - reinterpret_cast<ys_uint8*>(block.m_chunks));
        if (block.m_chunkSize == chunkSize)
        {
            if (byteOffset % chunkSize == 0)
            {
                ys_int32 idx = byteOffset / chunkSize;
                if (0 <= idx && idx < block.m_chunkCount)
                {
                    ysAssert(found == false);
                    found = true;
                    continue;
                }
            }
        }
        ysAssert(byteOffset < -chunkSize || block.m_chunkCount * chunkSize < byteOffset);
    }
    ysAssert(found);
}