#pragma once

#include "YoshiPBR/ysArrayG.h"

// http://dmitrysoshnikov.com/compilers/writing-a-pool-allocator/

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysMemoryPool
{
    enum
    {
        // All sizes are in bytes

        e_chunkSizeCount = 64,
        e_chunkSizeIncrement = 16,
        e_maxChunkSize = e_chunkSizeCount * e_chunkSizeIncrement,

        e_blockSize = 16 * e_maxChunkSize, // Upper bound on the size of the allocation for Block::m_cells
    };

    void Create();
    void Destroy();

    void* Allocate(ys_int32 size);
    void Free(void* obj, ys_int32 size);

    // For debugging
    void ValidateAllocation(void* obj, ys_int32 size) const;
    void ValidateBlocks() const;
    void ValidateFreeLists() const;

    struct Chunk
    {
        Chunk* m_nextInFreeList;
    };

    struct Block
    {
        Chunk* m_chunks;
        ys_int32 m_chunkCount;

        ys_int32 m_chunkSize;
    };

    ysArrayG<Block> m_blocks;
    Chunk* m_freeLists[e_chunkSizeCount];

    static ys_int32 s_chunkSizes[e_chunkSizeCount];
    static ys_int32 s_objSizeToChunkSizeIdx[1 + e_maxChunkSize];
    static bool s_staticDataInitialized;
};