#pragma once

#include "YoshiPBR/ysTypes.h"
#include <atomic>
#include <thread>

struct ysJob;
struct ysWorker;

// Must be a power of 2
#define ysJOBQUEUE_CAPACITY (16)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysJobQueue
{
    void SetToEmpty();

    // Push/Pop should only be called from the thread that owns this queue
    bool Push(ysJob*);
    ysJob* Pop();

    // Steal can be called from any thread.
    ysJob* Steal();

    ysJob* m_jobs[ysJOBQUEUE_CAPACITY];
    std::atomic<ys_uint32> m_head; // Only written to by threads that do NOT own this queue.
    std::atomic<ys_uint32> m_tail; // Only written to by the thread that owns this queue.

    ysWorker* m_owner;
};