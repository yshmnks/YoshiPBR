#include "ysJobQueue.h"

// https://manu343726.github.io/2017-03-13-lock-free-job-stealing-task-system-with-modern-c/
// https://blog.molecular-matters.com/2015/09/25/job-system-2-0-lock-free-work-stealing-part-3-going-lock-free/
// https://preshing.com/20120612/an-introduction-to-lock-free-programming/
// http://gcc.gnu.org/wiki/Atomic/GCCMM/AtomicSync
// https://en.cppreference.com/w/cpp/atomic/memory_order
// https://www.dre.vanderbilt.edu/~schmidt/PDF/work-stealing-dequeue.pdf

#define ysJOBQUEUE_MASK (ysJOBQUEUE_CAPACITY - 1)
ysAssertCompile((ysJOBQUEUE_CAPACITY & ysJOBQUEUE_MASK) == 0);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysJobQueue::Reset()
{
    m_head = 0;
    m_tail = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ysJobQueue::Push(ysJob* job)
{
    ysAssertDebug(std::this_thread::get_id() == m_threadId);

    ys_uint32 tail = m_tail.load(std::memory_order_relaxed);    // (0)
    ys_uint32 head = m_head.load(std::memory_order_relaxed);    // (1)
    ys_int32 count = tail - head;
    ysAssert(0 <= count && count <= ysJOBQUEUE_CAPACITY)
    if (count == ysJOBQUEUE_CAPACITY)
    {
        return false;
    }
    m_jobs[tail & ysJOBQUEUE_MASK] = job;
    m_tail.store(tail + 1, std::memory_order_release);          // (2)
    return true;

    // Remarks regarding memory ordering:
    // 0: Only this thread can write the tail, so beyond atomicity, no memory ordering precautions need to be taken when reading the tail
    //    from the same thread (compile-time and run-time reordering cannot change the behavior of single-threaded behavior).
    // 1: Only other threads can write the head. In the case that the pushed slot coincides with head, ensuring that the contents of the
    //    head slot can be safely overwritten is the responsibility of the stealing thread (which should have already extracted the job
    //    BEFORE incrementing the head).
    // 2: Enforce that the job is added to the ring buffer BEFORE incrementing the tail. If the order is swapped, then in the case that the
    //    queue is empty before this push, a stealing thread may falsely observe that the queue is not empty and prematurely read garbage.
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysJob* ysJobQueue::Pop()
{
    ysAssertDebug(std::this_thread::get_id() == m_threadId);

    // Let m be the actual head initially stored in memory
    // Let n be the actual tail initially stored in memory
    //
    // Let's imagine the necessary conditions for something to go wrong when this pop occurs concurrently with some steals. Firstly, it
    // suffices to only consider a single steal for each perceived head; this is because if two steals observe the same head, then only one
    // can succeed in the CAS increment to head. How can a second steal (call it Steal B) observe a different head? Like so...
    //     Steal A increments   m -> m+1
    //     Steal B loads        head = m+1
    //
    // Given that Steal B sees head as m+1, what can it perceive for tail? And what does this imply about what Pop percieves for head? There
    // are three scenarios (we will refer back to these scenarios repeatedly in the following analysis)...
    //
    //  1. Steal B sees tail as n. This means that Pop has not yet decremented tail and so Pop must see head as m+1
    //
    //     | Pop        | Steal A   | Steal B    |
    //     |------------|-----------|------------|
    //     |            | m -> m+1  |            |
    //     |------------|-----------|------------|
    //     |            |           | head = m+1 |
    //     |------------|-----------|------------|
    //     |            |           | tail = n   |
    //     |------------|-----------|------------|
    //     | n-> n-1    |           |            |
    //     | tail = n   |           |            |
    //     |------------|-----------|------------|
    //     | head = m+1 |           |            |
    //
    //     In this scenario, Pop and Steal B perceive IDENTICAL [head, tail] pair [m+1, n].
    //
    //  2. Steal B sees tail as n-1 and Pop sees head as m
    //
    //     | Pop        | Steal A   | Steal B    |
    //     |------------|-----------|------------|
    //     | n-> n-1    |           |            |
    //     | tail = n   |           |            |
    //     |------------|-----------|------------|
    //     | head = m   |           |            |
    //     |------------|-----------|------------|
    //     |            | m -> m+1  |            |
    //     |------------|-----------|------------|
    //     |            |           | head = m+1 |
    //     |------------|-----------|------------|
    //     |            |           | tail = n-1 |
    //
    //     In this scenario, Pop perceives [m, n] and Steal B percieves [m+1, n-1]
    //
    //  3. Steal B sees tail as n-1 and Pop sees head as m+1
    //
    //     | Pop        | Steal A   | Steal B    |
    //     |------------|-----------|------------|
    //     |            | m -> m+1  |            |
    //     |------------|-----------|------------|
    //     |            |           | head = m+1 |
    //     |------------|-----------|------------|
    //     | n-> n-1    |           |            |
    //     | tail = n   |           |            |
    //     |------------|-----------|------------|
    //     |            |           | tail = n-1 | <----
    //     |------------|-----------|------------|      |---- These two can also be swapped, it makes no difference for our analysis.
    //     | head = m+1 |           |            | <----
    //
    //    In this scenario, Pop perceives [m+1, n] and Steal B perceives [m+1, n-1]
    //
    // The case where Pop sees the queue as already empty is trivial; the emptiness reflects the most recent reality (Push can only come
    // from the same thread) and so Pop can only return NULL. Next we consider the non-trivial cases, letting t be the tail as seen by Pop.
    //
    // What if Pop sees head == tail-1 ( i.e. [head, tail]_Pop = [t-1, t] )?
    //   Scenario 1: Pop and Steal B both observe correctly that they are acquiring the last job. It suffices to race for it via atomic CAS.
    //   Scenario 2: Steal B perceives [t, t-1] (it perceives head as having exceeded tail) and so it doesn't attempt to steal. Now here
    //               comes the crucial observation! Pop actually sees an outdated state. Steal A has already stolen the last entry. If Pop,
    //               attempts to acquire the last job not by decrementing tail, but rather by incrementing head, then we can guard against
    //               Pop accessing garbage by racing against Steal A for head... again via CAS.
    //   Scenario 3: Steal B perceives [t-1, t-1] and fogoes the steal. Pop correctly sees the most recent state and can safely proceed.
    // As can be seen, scenarios 1 and 2 both require care. Scenario 2 is more problematic and behooves us to acquire the last job via CAS
    // on head specifically (CAS on tail wouldn't cut it). Naturally, CAS on tail also handles scenario 1.
    //
    // what if Pop sees head == tail-2 ( i.e. [head, tail]_Pop = [t-2, t] )?
    //   Scenario 1: Steal B also percieves that 2 jobs remain, and so proceeds to steal. The perceived state reflects the latest reality
    //               so everything is hunky dory.
    //   Scenario 2: Steal B perceives [t-1, t-1] and so skips the steal. Although Pop is mistaken and there is actually only a single job
    //               left, Pop grabs from the tail end safely because there is no contention from Steal A acting on the head end when there
    //               were still two jobs remaining.
    //   Scenario 3: Steal B perceives [t-2, t-1] and it steals the single job it sees. Pop grabs from the tail and the queue is emptied
    //               without contention between Pop and Steal B (Pop's reality is the truth, there were actually 2 jobs remaining).
    // As can be scene, there are no precautions to take here. Either Pop correctly sees 2 jobs remaining (Scenarios 1, 3)... or Pop is
    // mistaken (there is actually only a single job remaining), but Steal B serendipitously perceives that there is nothing to steal.
    //
    // Clearly, if Pop sees more than 2 jobs remaining in the queue then there is nothing more to worry about. Note that in all our analysis
    // it is ESSENTIAL that memory ordering is such that...
    //   - Pop read-decrements tail BEFORE reading head.
    //   - Steal reads head BEFORE reading tail.
    //
    // This concludes the explanation for the implementation for Pop.

    // Definitely need ACQUIRE to prevent reordering with the read of head. But do I also need RELEASE? I don't think so...?
    ys_uint32 tail = m_tail.fetch_sub(1, std::memory_order_acquire);
    ys_uint32 head = m_head.load(std::memory_order_acquire); // We might race for the head, so make sure the head slot has also been written
    ysAssert(head <= tail);
    if (head == tail)
    {
        // Already empty
        m_tail.store(head, std::memory_order_relaxed);
        return nullptr;
    }

    ysJob* job = m_jobs[(tail - 1) % ysJOBQUEUE_MASK];
    if (tail + 1 < head)
    {
        // there's still more than one item in the queue
        return job;
    }

    // Race against Steal for the last job.
    bool success = m_head.compare_exchange_weak(head, head + 1, std::memory_order_release);
    return success ? job : nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysJob* ysJobQueue::Steal()
{
    ysAssertDebug(std::this_thread::get_id() != m_threadId);

    // It is crucial when reading tail that we use ACQUIRE memory ordering in tandem with the RELEASE ordering for the write to tail in
    // "Push." It's a bit subtle, but while RELEASE on its own prevents compiler and run-time reordering in Push, that alone makes no
    // promise that this stealing thread will observe the updated contents of the pushed slot as a side effect of the tail increment.
    // Pairing ACQUIRE with RELEASE guarantees that the slot's contents are as recent as is consistent with the tail we read, which avoids
    // a disaster when head==tail-1.
    //
    // We must also use ACQUIRE ordering when reading the head to prevent the tail from being read before the head. The reasons are somewhat
    // complex (see the comments for Pop).
    ys_uint32 head = m_head.load(std::memory_order_acquire);
    ys_uint32 tail = m_tail.load(std::memory_order_acquire);
    ys_int32 count = tail - head;
    ysAssert(count <= ysJOBQUEUE_CAPACITY);
    if (head >= tail)
    {
        return nullptr;
    }
    // Obtain the speculated (it may have already been stolen by another thread) head job AND ONLY THEN increment the head via atomic CAS
    // (this order of operations is enforced via std::memory_order_release). Without this strict ordering, "Push" could conceivably observe
    // (when the queue is at capacity) what we believe to be the head as the incremented tail, enqueue a new job into the slot we intend
    // to steal, catastrophically leading to the same job being run on multiple threads.
    ysJob* job = m_jobs[head & ysJOBQUEUE_MASK];
    bool success = m_head.compare_exchange_weak(head, head + 1, std::memory_order_release);
    if (success == false)
    {
        // Our percieved head is stale. Somebody beat us to it (either "Steal" from another thread or "Pop" from the owning thread).
        return nullptr;
    }
    return job;
}