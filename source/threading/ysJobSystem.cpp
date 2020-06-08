#include "ysJobSystem.h"
#include "YoshiPBR/ysMemoryPool.h"
#include "YoshiPBR/ysThreading.h"
#include <thread>

struct ysJob;
struct ysJobQueue;
struct ysWorker;
struct ysJobSystem;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysJob
{
    void Create(ysWorker* worker, const ysJobDef&);

    void Execute(ysJobSystem*);
    bool IsFinished() const;

    void __Finish();

    ysJobFcn* m_fcn;
    void* m_fcnArg;
    ysJob* m_parent;
    // The original worker that submitted this job. This is necessary for freeing this job as we allocated it from that worker's mem pool.
    ysWorker* m_owner;
    std::atomic<ys_int32> m_unfinishedJobCount;

    /////////////
    // PADDING //
    /////////////
    struct PayloadFormat
    {
        ysJobFcn* a;
        void* b;
        ysJob* c;
        ysWorker* d;
        std::atomic<ys_int32> e;
    };
    static constexpr std::size_t PAYLOAD_SIZE = sizeof(PayloadFormat);
    static constexpr std::size_t FALSE_SHARING_SIZE = std::hardware_destructive_interference_size;
    ys_uint8 _PAD[FALSE_SHARING_SIZE - PAYLOAD_SIZE];
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysJobQueue
{
    enum
    {
        e_jobCapacity = 64, // Must be a power of 2
        e_jobIdxMask = e_jobCapacity - 1,
    };
    ysAssertCompile((e_jobCapacity & e_jobIdxMask) == 0);

    void SetToEmpty();

    // Push/Pop should only be called from the thread that owns this queue
    bool Push(ysJob*);
    ysJob* Pop();

    // Steal can be called from any thread.
    ysJob* Steal();

    ysJob* m_jobs[e_jobCapacity];
    std::atomic<ys_uint32> m_head; // Only written to by threads that do NOT own this queue.
    std::atomic<ys_uint32> m_tail; // Only written to by the thread that owns this queue.

    ysWorker* m_owner;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysWorker
{
    enum struct Mode
    {
        e_undefined,
        e_foreground,
        e_background,
    };

    enum struct State : ys_uint8
    {
        e_idle,
        e_spinning,
        e_killed,
    };

    void CreateInForeground(ysJobSystem*);
    void CreateInBackground(ysJobSystem*);
    void Destroy();

    void Submit(ysJob* job);
    void Wait(ysJob* job);

    ysJob* GetJob();

    // This is the loop run by BACKGROUND workers.
    // The FOREGROUND thread also has a corresponding worker, but never sleeps. Once it has finished submitting jobs, it "waits" and joins
    // in on the work juggling until all workers' job queues (including its own) are emptied, at which point the foreground thread resumes.
    void SleepUntilAlarm();

    ysJobSystem* m_manager;
    ysJobQueue m_jobQueue;
    ysThread m_thread;
    std::thread::id m_threadId;
    Mode m_mode;
    std::atomic<State> m_state;

    // Jobs and Job-function-user-data must persist in memory until the Job is executed. To prevent memory fragmentation, users should
    // prefer to pool allocate such (typically) small objects. Each worker maintains its own memory pool to reduce heap contention.
    // TODO: Due to job-stealing, we must lock the mem pool when allocating/freeing jobs. It seems this should still be better than sharing
    //       a single memory resource owned by the main thread. However, it remains to be profiled how bad lock contention can get.
    ysMemoryPool m_memPool;
    ysLock m_memPoolLock; // Freeing jobs reaches across thread boundaries, so we need this lock.
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysJobSystem
{
    enum
    {
        e_workerCapacity = 64
    };

    void Create(const ysJobSystemDef&);
    void Destroy();

    ysWorker* GetWorkerForThisThread();
    ysJob* StealJobForPerpetrator(const ysWorker* perpetrator);

    ysWorker m_workers[e_workerCapacity];
    ys_int32 m_workerCount;
    ysSemaphore m_alarmSemaphore;

    std::atomic<bool> m_isShuttingDown;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ysJob
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ysJob::Create(ysWorker* owner, const ysJobDef& def)
{
    ysAssert(def.m_fcn != nullptr);
    m_fcn = def.m_fcn;
    m_fcnArg = def.m_fcnArg;
    m_parent = def.m_parentJob;
    m_owner = owner;
    m_unfinishedJobCount.store(1, std::memory_order_release);
    if (m_parent != nullptr)
    {
        m_parent->m_unfinishedJobCount.fetch_add(1, std::memory_order_release);
    }
}

void ysJob::Execute(ysJobSystem* sys)
{
    m_fcn(sys, this, m_fcnArg);
    __Finish();
}

bool ysJob::IsFinished() const
{
    ys_int32 count = m_unfinishedJobCount.load(std::memory_order_acquire);
    return (count == 0);
}

void ysJob::__Finish()
{
    ys_int32 count = m_unfinishedJobCount.fetch_sub(1, std::memory_order_acq_rel);
    ysAssert(count >= 1);
    if (count == 1)
    {
        if (m_parent != nullptr)
        {
            m_parent->__Finish();
        }

        {
            // Is it safe to free the job at this point? Could it still be referenced by other jobs?
            ysScopedLock lock(&m_owner->m_memPoolLock);
            m_owner->m_memPool.Free(this, sizeof(ysJob));
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ysJobQueue
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// https://blog.molecular-matters.com/2015/09/25/job-system-2-0-lock-free-work-stealing-part-3-going-lock-free/
// https://manu343726.github.io/2017-03-13-lock-free-job-stealing-task-system-with-modern-c/
// https://preshing.com/20120612/an-introduction-to-lock-free-programming/
// https://www.dre.vanderbilt.edu/~schmidt/PDF/work-stealing-dequeue.pdf
// https://en.cppreference.com/w/cpp/atomic/memory_order
// http://gcc.gnu.org/wiki/Atomic/GCCMM/AtomicSync

void ysJobQueue::SetToEmpty()
{
    // 1 instead of 0 because of the preemptive tail decrement in Pop
    m_head = 1;
    m_tail = 1;
}

bool ysJobQueue::Push(ysJob* job)
{
    ysAssert(std::this_thread::get_id() == m_owner->m_threadId);

    ys_uint32 tail = m_tail.load(std::memory_order_relaxed);    // (0)
    ys_uint32 head = m_head.load(std::memory_order_acquire);    // (1)
    ys_int32 count = tail - head;
    ysAssert(0 <= count && count <= e_jobCapacity);
    if (count == e_jobCapacity)
    {
        return false;
    }
    m_jobs[tail & e_jobIdxMask] = job;
    m_tail.store(tail + 1, std::memory_order_release);          // (2)
    return true;

    // Remarks regarding memory ordering:
    // 0: Only this thread can write the tail, so beyond atomicity, no memory ordering precautions need to be taken when reading the tail
    //    from the same thread (compile-time and run-time reordering cannot change the behavior of single-threaded behavior).
    // 1: Without ACQUIRE ordering, there is no guarantee that a RELEASE write on the head from Steal will be immediately visible.
    // 2: Enforce that the job is added to the ring buffer BEFORE incrementing the tail. If the order is swapped, then in the case that the
    //    queue is empty before this push, a stealing thread may falsely observe that the queue is not empty and prematurely read garbage.
}

ysJob* ysJobQueue::Pop()
{
    ysAssert(std::this_thread::get_id() == m_owner->m_threadId);

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
    // ... Let [m, n] be the actual [head, tail] initially stored in memory when Pop read-decrements tail
    // ... Let s be the number of Steals that succeed between Pop's reads of tail and head, such that Pop perceives head as m+s
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

    // ACQUIRE to prevent reordering with the immediately proceeding read of head.
    // RELEASE to prevent Steals from {A} from reading the pre-decremeted tail.
    ys_uint32 tail = m_tail.fetch_sub(1, std::memory_order_acq_rel);
    // We might race for the head. Make sure we also see the new contents of head slot written by Steal.
    ys_uint32 head = m_head.load(std::memory_order_acquire);
    ys_int32 count = tail - head;
    ysAssert(count >= 0);
    if (count == 0)
    {
        // Already empty. Only this thread writes the tail and there are no other writes preceding this one, so RELAXED ordering is safe.
        // However, to prevent Steals from spuriously failing while this tail write trickles to other threads, use RELEASE ordering.
        m_tail.store(head, std::memory_order_release);
        return nullptr;
    }

    ysJob* job = m_jobs[(tail - 1) & e_jobIdxMask];
    if (count >= 2)
    {
        // there's still more than one item in the queue
        return job;
    }

    // Race against Steal for the last job. RELEASE ordering makes the head increment immediately visible to Steals... I think RELAXED
    // should technically be safe. As I understand it, CAS compares against the true value in memory. So any Steals that read the old head
    // due to RELAXED latency cannot acquire a garbage job. However, the latency also means that new Steals that would have otherwise
    // succeeded (had they observed the latest head) might fail. It feels more correct to avoid spurious Steal failures.
    bool success = m_head.compare_exchange_weak(head, head + 1, std::memory_order_release);
    // RELEASE to prevent head increment from occuring after tail restoration; also make this write immediately visible to Steal.
    m_tail.store(tail, std::memory_order_release);
    return success ? job : nullptr;
}

ysJob* ysJobQueue::Steal()
{
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
    YS_REF(count);
    ysAssert(count <= e_jobCapacity);
    if (head >= tail)
    {
        return nullptr;
    }
    // Obtain the speculated (it may have already been stolen by another thread) head job AND ONLY THEN increment the head via atomic CAS
    // (this order of operations is enforced via std::memory_order_release). Without this strict ordering, "Push" could conceivably observe
    // (when the queue is at capacity) what we believe to be the head as the incremented tail, enqueue a new job into the slot we intend
    // to steal, catastrophically leading to the same job being run on multiple threads.
    ysJob* job = m_jobs[head & e_jobIdxMask];
    bool success = m_head.compare_exchange_weak(head, head + 1, std::memory_order_release);
    if (success == false)
    {
        // Our percieved head is stale. Somebody beat us to it (either "Steal" from another thread or "Pop" from the owning thread).
        return nullptr;
    }
    return job;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ysWorker
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void sBackgroundThreadFcn(void* workerPtr)
{
    ysWorker* worker = static_cast<ysWorker*>(workerPtr);
    worker->SleepUntilAlarm();
}

void ysWorker::CreateInForeground(ysJobSystem* sys)
{
    m_mode = Mode::e_foreground;
    m_state.store(State::e_idle, std::memory_order_release);
    m_manager = sys;
    m_jobQueue.m_owner = this;
    m_jobQueue.SetToEmpty();
    m_thread.Reset();
    m_threadId = std::this_thread::get_id();
    m_memPool.Create();
    m_memPoolLock.Reset();
}

void ysWorker::CreateInBackground(ysJobSystem* sys)
{
    m_mode = Mode::e_background;
    m_state.store(State::e_idle, std::memory_order_release);
    m_manager = sys;
    m_jobQueue.m_owner = this;
    m_jobQueue.SetToEmpty();
    m_thread.Create(sBackgroundThreadFcn, this);
    m_threadId = m_thread.GetID();
    m_memPool.Create();
    m_memPoolLock.Reset();
}

void ysWorker::Destroy()
{
    m_thread.Destroy();
    ysAssert(m_jobQueue.m_head == m_jobQueue.m_tail);
    m_threadId = std::thread::id();
    m_memPool.Destroy();
}

void ysWorker::Submit(ysJob* job)
{
    bool success = m_jobQueue.Push(job);
    if (success)
    {
        m_manager->m_alarmSemaphore.Signal(m_manager->m_workerCount - 1);
    }
    else
    {
        job->Execute(m_manager);
    }
}

void ysWorker::Wait(ysJob* blockingJob)
{
    ysAssert(m_mode == Mode::e_foreground || m_state.load(std::memory_order_relaxed) == State::e_spinning);
    while (blockingJob->IsFinished() == false)
    {
        ysJob* job = GetJob();
        if (job != nullptr)
        {
            job->Execute(m_manager);
        }
    }
}

ysJob* ysWorker::GetJob()
{
    ysJob* ownJob = m_jobQueue.Pop();
    if (ownJob != nullptr)
    {
        return ownJob;
    }

    ysJob* stolenJob = m_manager->StealJobForPerpetrator(this);
    if (stolenJob == nullptr)
    {
        std::this_thread::yield();
        return nullptr;
    }
    return stolenJob;
}

void ysWorker::SleepUntilAlarm()
{
    // Wait until work exists
    ysAssert(m_state.load(std::memory_order_relaxed) == State::e_idle);
    m_manager->m_alarmSemaphore.Wait();

    while (true)
    {
        m_state.store(State::e_spinning, std::memory_order_release);
        ysJob* job = GetJob();
        while (job != nullptr)
        {
            job->Execute(m_manager);
            job = GetJob();
        }

        // Is it possible for us to read value that is more stale than the default FALSE set on JobSystem creation? This concern is why we
        // ACQUIRE. Correctness aside, it would be more optimal for the steady state to use a RELAXED load and on shutdown, continually poke
        // the alarm until all workers enter the killed state. I suppose ACQUIRE also has its advantages though; we only need to poke the
        // alarm once on shutdown.
        bool mgrIsShuttingDown = m_manager->m_isShuttingDown.load(std::memory_order_acquire);
        if (mgrIsShuttingDown)
        {
            m_state.store(State::e_killed, std::memory_order_release);
            break;
        }

        // Sleepy time. If another job gets pushed into any worker's queue we will wake up.
        m_state.store(State::e_idle, std::memory_order_release);
        m_manager->m_alarmSemaphore.Wait();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ysJobSystem
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ysJobSystem::Create(const ysJobSystemDef& def)
{
    ysAssert(1 <= def.m_workerCount && def.m_workerCount <= e_workerCapacity);
    m_isShuttingDown.store(false, std::memory_order_release);
    m_alarmSemaphore.Create(0);
    m_workerCount = def.m_workerCount;
    m_workers[0].CreateInForeground(this);
    for (ys_int32 i = 1; i < m_workerCount; ++i)
    {
        m_workers[i].CreateInBackground(this);
    }
}

void ysJobSystem::Destroy()
{
    ysAssert(std::this_thread::get_id() == m_workers[0].m_threadId);
    m_isShuttingDown.store(true, std::memory_order_release);
    m_alarmSemaphore.Signal(m_workerCount - 1);

    bool anyBackgroundWorkerStillAlive = true;
    while (anyBackgroundWorkerStillAlive)
    {
        ysJob* job = m_workers[0].GetJob();
        if (job != nullptr)
        {
            job->Execute(this);
        }
        // It is possible for this thread to run out of jobs to steal and even for all worker's to empty their respective queues, only for
        // a still inflight job to push more jobs. So we need to query all workers' states to exit this loop. Once all workers are killed
        // we can be sure that not only are all job queues empty, but also that there are no more jobs in progress.
        anyBackgroundWorkerStillAlive = false;
        for (ys_int32 i = 1; i < m_workerCount; ++i)
        {
            ysWorker::State wkrState = m_workers[i].m_state.load(std::memory_order_acquire);
            if (wkrState != ysWorker::State::e_killed)
            {
                anyBackgroundWorkerStillAlive = true;
                break;
            }
        }
    }

    for (ys_int32 i = 0; i < m_workerCount; ++i)
    {
        m_workers[i].Destroy();
    }
    m_workerCount = 0;

    m_alarmSemaphore.Destroy();
}

ysWorker* ysJobSystem::GetWorkerForThisThread()
{
    std::thread::id id = std::this_thread::get_id();
    ysAssert(m_workerCount > 0);
    for (ys_int32 i = 0; i < m_workerCount; ++i)
    {
        if (m_workers[i].m_threadId == id)
        {
            return m_workers + i;
        }
    }
    ysAssert(false); // Calling this API is a mistake if the worker could not be found.
    return nullptr;
}

ysJob* ysJobSystem::StealJobForPerpetrator(const ysWorker* perpetrator)
{
    ys_int32 perpetratorIdx = ys_int32(perpetrator - m_workers);
    ysAssert(0 <= perpetratorIdx && perpetratorIdx < m_workerCount);
    for (ys_int32 i = 1; i < m_workerCount; ++i)
    {
        ys_int32 victimIdx = (perpetratorIdx + i) % m_workerCount;
        ysJob* loot = m_workers[victimIdx].m_jobQueue.Steal();
        if (loot != nullptr)
        {
            return loot;
        }
    }
    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysJobSystem* ysJobSystem_Create(const ysJobSystemDef& def)
{
    ysJobSystem* sys = static_cast<ysJobSystem*>(ysMalloc(sizeof(ysJobSystem)));
    sys->Create(def);
    return sys;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysJobSystem_Destroy(ysJobSystem* sys)
{
    sys->Destroy();
    ysFree(sys);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysJob* ysJobSystem_CreateJob(ysJobSystem* sys, const ysJobDef& def)
{
    ysWorker* wkr = sys->GetWorkerForThisThread();
    ysJob* job;
    {
        ysScopedLock lock(&wkr->m_memPoolLock);
        job = static_cast<ysJob*>(wkr->m_memPool.Allocate(sizeof(ysJob)));
    }
    job->Create(wkr, def);
    return job;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysJobSystem_SubmitJob(ysJobSystem* sys, ysJob* job)
{
    ysWorker* wkr = sys->GetWorkerForThisThread();
    wkr->Submit(job);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysJobSystem_WaitOnJob(ysJobSystem* sys, ysJob* job)
{
    ysWorker* wkr = sys->GetWorkerForThisThread();
    wkr->Wait(job);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysJobSystemAllocation ysJobSystem_Allocate(ysJobSystem* sys, ys_int32 byteCount)
{
    ysJobSystemAllocation alloc;
    alloc.m_worker = sys->GetWorkerForThisThread();
    {
        ysScopedLock lock(&alloc.m_worker->m_memPoolLock);
        alloc.m_dataPtr = alloc.m_worker->m_memPool.Allocate(byteCount);
    }
    return alloc;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysJobSystem_Free(ysJobSystemAllocation* alloc, ys_int32 byteCount)
{
    ysWorker* wkr = alloc->m_worker;
    {
        ysScopedLock lock(&wkr->m_memPoolLock);
        wkr->m_memPool.Free(alloc->m_dataPtr, byteCount);
    }
    alloc->m_dataPtr = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ysJobSystem_AreResourcesEmptied(ysJobSystem* sys)
{
    for (ys_int32 i = 0; i < sys->m_workerCount; ++i)
    {
        ysWorker* wkr = &sys->m_workers[i];
        ysScopedLock lock(&wkr->m_memPoolLock);
        bool wkrMemPoolEmpty = wkr->m_memPool.IsEmpty();
        if (wkrMemPoolEmpty == false)
        {
            return false;
        }
    }
    return true;
}