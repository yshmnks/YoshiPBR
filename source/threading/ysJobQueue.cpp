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
    // 1: While other threads can write the head, in the case that the pushed slot coincides with head, ensuring that the contents of the
    //    head slot can be safely overwritten is the responsibility of the stealing thread (which should have already extracted the job
    //    BEFORE incrementing the head via RELEASE memory ordering).
    // 2: Enforce that the job is added to the ring buffer BEFORE incrementing the tail. If the order is swapped, then in the case that the
    //    queue is empty before this push, a stealing thread may falsely observe that the queue is not empty and prematurely read garbage.
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysJob* ysJobQueue::Pop()
{
    ysAssertDebug(std::this_thread::get_id() == m_threadId);

    // Assume certain aspects of the implementation a priori. Namely...
    //   - Pop first read-decrements tail and only then reads head.
    //   - Steal reads in the opposite order, head first and then tail.
    // We shall justify the lock-free implementation for Pop under these assumptions.
    //
    // Our goal is to prevent both Pop and Steal from trying to pull jobs from an already empty queue due to outdated perception of the
    // [head, tail] pair. Let's imagine potentially problematic ways to interleave operations between a single Pop and multiple Steals.
    //
    // Steals can be split into two categories, those that read tail before it is decremented by Pop and those that read it after. Let us
    // label the former subset of Steals with {A} and the latter with {B}.
    // - Steals from {A} cannot interact nefariously with Pop. Pop's preemptive tail decrement is precautionary; regardless of whether Pop
    //   succeeds, such Steals alone cannot drop the queue below occupancy one (unless the queue is already empty, which is the trivial case
    //   where Pop fails with confidence). Therefore, in the absence of other types of Steals, Pop always has something to acquire.
    // - Steals from {B} are potential troublemakers. Because they read the pre-decremented tail these Steals can empty the queue. Care must
    //   be taken to ensure Pop fails acquisition if it is late to the party. The CRUCIAL OBSERVATION THAT SAVES OUR BACON here is that
    //   relative to Pop, these Steals see not only earlier snapshots of tail but also earlier snapshots of head (recall our aforementioned
    //   assumptions regarding memory ordering). Therefore, Pop can safely make certain inferences about the end result of these Steals
    //   based on what it itself observes for [head, tail]...
    //   - Pop observes two (or more) jobs remaining: Steals from {B} can only take the headmost of the two jobs. Combined with our earlier
    //     observation that Steals from {A} cannot drop the occupancy below one, this means Pop can grab from the tail end with impunity.
    //   - Pop observes one job remaining: Steals from {B} can be in contention for this last job, so we race for it. Since stealing always
    //     occurs at the head end, we should compete to increment head (as opposed to decrementing the tail).
    //   - Pop observes no jobs remaining: This observation by Pop always reflects reality, since jobs are only ever Pushed from the same
    //     thread. So Pop should simply fails to acquire a job.
    // To make all of this more concrete we provide the following illustration for the relation between Pop, {A}, and {B}:
    // ... Let m be the actual head initially stored in memory
    // ... Let n be the actual tail initially stored in memory
    // ... Let s be the number of Steals that succeed before Pop reads head, such that Pop perceives head as m+s
    //     ______________________________________________________
    //    | Pop:       | Steals from {A}:     | Steals from {B}: |
    //    |------------|----------------------|------------------|
    //    |            |                      | head <= m+s      |
    //    |------------|----------------------|------------------|
    //    |            |                      | tail = n         |
    //    |------------|----------------------|------------------|
    //    | n -> n-1   |                      |                  |
    //    | tail = n   |                      |                  |
    //    |------------|----------------------|------------------|
    //    |            | tail = n-1           |                  |
    //    |            | (head read upstream) |                  |
    //    |------------|----------------------|------------------|
    //    | head = m+s |                      |                  |
    //
    // This concludes the explanation for the implementation of Pop below.

    // Definitely need ACQUIRE to prevent reordering with the read of head. But do I also need RELEASE? I don't think so...?
    ys_uint32 tail = m_tail.fetch_sub(1, std::memory_order_acquire);
    ys_uint32 head = m_head.load(std::memory_order_acquire); // We might race for the head. Make sure the head slot has also been written.
    ys_int32 count = tail - head;
    ysAssert(count >= 0);
    if (count == 0)
    {
        // Already empty. Only this thread writes the tail and there are no other writes preceding this one, so RELAXED ordering suffices.
        m_tail.store(head, std::memory_order_relaxed);
        return nullptr;
    }

    ysJob* job = m_jobs[(tail - 1) % ysJOBQUEUE_MASK];
    if (count >= 2)
    {
        // there's still more than one item in the queue
        return job;
    }

    // Race against Steal for the last job. Unlike with Steal, RELAXED ordering suffices. Steal used RELEASE ordering to guard against a
    // concurrent Push from rendering the head job stale; However, Push/Pop must come from the same thread, so this isn't a problem here.
    bool success = m_head.compare_exchange_weak(head, head + 1, std::memory_order_relaxed);
    m_tail.store(tail + 1, std::memory_order_release); // RLEASE to prevent head increment from occuring after tail restoration.
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