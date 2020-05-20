#pragma once

#include "YoshiPBR/ysTypes.h"
#include <atomic>
#include <thread>

struct ysJob;

// Must be a power of 2
#define ysJOBQUEUE_CAPACITY (16)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysJobQueue
{
    void Reset();

    // Push/Pop should only be called from the thread that owns this queue
    bool Push(ysJob*);
    ysJob* Pop();

    // Steal should only be called from a thread that does NOT own this queue
    ysJob* Steal();

    ysJob* m_jobs[ysJOBQUEUE_CAPACITY];
    std::atomic<ys_uint32> m_head; // Only written to by threads that do NOT own this queue.
    std::atomic<ys_uint32> m_tail; // Only written to by the thread that owns this queue.

#if ysDEBUG_BUILD
    std::thread::id m_threadId;
#endif
};