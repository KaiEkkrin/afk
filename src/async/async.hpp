/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see [http://www.gnu.org/licenses/].
 */

#ifndef _AFK_ASYNC_ASYNC_H_
#define _AFK_ASYNC_ASYNC_H_

#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "thread_allocation.hpp"
#include "work_queue.hpp"

/* Don't enable this unless you want MAXIMAL SPAM */
#define ASYNC_DEBUG_SPAM 0

#define ASYNC_WORKER_DEBUG 0

#if ASYNC_DEBUG_SPAM
#include <ctime>
#include <iostream>
#include "../clock.hpp"

extern std::mutex debugSpamMut;

#define ASYNC_DEBUG(chain) \
    { \
        std::unique_lock<std::mutex> guard(debugSpamMut); \
        afk_clock::time_point debugTime = \
            afk_clock::now(); \
        afk_out << "in thread " << std::this_thread::get_id() << " at " << debugTime.time_since_epoch().count() << ": "; \
        afk_out << chain << std::endl; \
    }

#define ASYNC_CONTROL_DEBUG(chain) \
    { \
        std::unique_lock<std::mutex> guard(debugSpamMut); \
        afk_clock::time_point debugTime = \
            afk_clock::now(); \
        afk_out << "control " << this << " in thread "; \
        afk_out << std::this_thread::get_id() << " at " << debugTime.time_since_epoch().count() << ": "; \
        afk_out << chain << std::endl; \
    }

#else
#define ASYNC_DEBUG(chain)
#define ASYNC_CONTROL_DEBUG(chain)
#endif /* ASYNC_DEBUG_SPAM */

/* Asynchronous function calling for AFK.  Passes out the function to
 * a threadpool.  Supports tail-recursive / multiply tail-recursive
 * behaviour (just have the function make another async call in the
 * same call group).
 * Call groups are entirely separate so that I can wait for one and
 * ignore (some day suspend?) another.  I hope that the OS won't mind
 * that I spin up lots of busy threads.  (Feh, finesse.)
 */


class AFK_AsyncException: public std::exception {};


/* Here are the mechanisms an AsyncGang uses to control its workers */
class AFK_AsyncControls
{
private:
    /* It's really important this thing doesn't get accidentally
     * duplicated
     */
    AFK_AsyncControls(const AFK_AsyncControls& controls) = delete;
    AFK_AsyncControls& operator=(const AFK_AsyncControls& controls) = delete;

protected:
    /* Here's the list of thread IDs of the workers (the size of this
     * list is, of course, the worker count.)
     */
    std::vector<unsigned int> workerIds;

    uint64_t allWorkerMask;

    /* The workers wait on this condition variable until things are
     * primed and they're ready to go. Here's how we do it:
     * - The control thread flags a bit of `workReady' for each
     * worker it wants to run
     * - When a worker grabs the condition variable, it clears its
     * bit to indicate it got it.  It then waits on the variable
     * again until all bits become 0, indicating all workers are
     * sync'd up and ready to go
     * - To latch the workers out, I use `workRunning' with the
     * same scheme: when the workers start, they set their bit
     * of `workRunning'.  When they decide things are finished, they
     * clear their bit and wait until all the bits become zero.
     */
    std::condition_variable workCond;
    std::mutex workMut;
    uint64_t workReady;
    uint64_t workRunning;
    bool quit; /* Tells the workers to instead quit out entirely */

public:
    AFK_AsyncControls(const std::vector<unsigned int>& _workerIds):
        workerIds(_workerIds), allWorkerMask(0), workReady(0), workRunning(0), quit(false)
    {
        for (auto id : _workerIds) allWorkerMask |= (1ull << id);
    }

    void control_workReady(void);
    void control_quit(void);

    /* Returns true if there is work to be done, false for quit */
    bool worker_waitForWork(unsigned int id);

    /* Blocks until all workers are latched out. */
    void worker_waitForFinished(unsigned int id);

    /* Checks whether all work is finished or not. */
    bool control_workFinished(void);
};


/* A wrapper for the thread-local type (below) to control copying
 * it around.
 */
template<typename ThreadLocalType>
class AFK_ThreadLocalWrapper
{
public:
    ThreadLocalType inner;
    std::mutex mut;
};

/* TODO: Parameter for this function, to stop it from having
 * to reference globals?
 */
typedef std::function<bool (void)> AFK_AsyncTaskFinishedFunc;

/* Here's how it works:
 * - ParameterType is the type of the work queue parameter, which
 * includes each function to call along with its arguments (see
 * work_queue).
 * - ReturnType is the type the called function should return.
 * (Barely used; I've been sticking to bool.)
 * - ThreadLocalType is the type of a field whose contents
 * should be copied into thread-local storage at the start of each
 * run, and a const reference passed to the called function.
 * (It should be quite small, because it goes on the stack, and
 * have a trivial constructor, and be copy constructable and
 * assignable.  It doesn't need its own concurrency control.)
 * - AsyncTaskFinishedFunc is called periodically (not on any
 * deterministic schedule) to decide when things are finished.
 */
template<typename ParameterType, typename ReturnType, typename ThreadLocalType, AFK_AsyncTaskFinishedFunc& taskFinished>
void afk_asyncWorker(
    AFK_AsyncControls& controls,
    AFK_ThreadLocalWrapper<ThreadLocalType>& threadLocalSource,
    unsigned int id,
    bool first,
    AFK_WorkQueue<ParameterType, ReturnType, ThreadLocalType>& queue,
    std::promise<ReturnType>*& promise)
{
    while (controls.worker_waitForWork(id))
    {
        /* Copy out the thread-local values for this run. */
        ThreadLocalType tl;

        threadLocalSource.mut.lock();
        tl = threadLocalSource.inner; /* that seriously shouldn't throw an exception */
        threadLocalSource.mut.unlock();

        /* TODO Do something sane with the return values rather
         * than just accumulating the last one!
         */
        ReturnType retval;
        enum AFK_WorkQueueStatus status;
        bool finished = false;

        while (!finished)
        {
            status = queue.consume(id, tl, retval);
            switch (status)
            {
            case AFK_WQ_BUSY:
                /* Keep looping. */
                break;

            case AFK_WQ_WAITING:
                /* Check for finished. */
                if (taskFinished())
                {
                    finished = true;
                }
                else
                {
                    /* Give way so I don't cram the CPU with busy-waits.
                     * This is really important -- the whole system chokes
                     * if I don't do it.
                     */
                    std::this_thread::yield();
                }
                break;

            default:
                /* Programming error. */
                throw AFK_AsyncException();
            }
        }

        /* I think I've finished.  Sync up. */
        controls.worker_waitForFinished(id);

#if ASYNC_WORKER_DEBUG
        afk_out << "X";
#endif

        /* At this point, everyone has finished.
         * See TODO above: for expediency, if I've got the `first' flag
         * (only one worker has), I'll populate the return value, otherwise
         * I'll throw it away now
         */
        if (first)
        {
            promise->set_value(retval);
#if ASYNC_DEBUG_SPAM
            ASYNC_DEBUG("fulfilling promise " << std::hex << (void *)promise)
#endif
        }
    }
}


template<typename ParameterType, typename ReturnType, typename ThreadLocalType, AFK_AsyncTaskFinishedFunc& taskFinished>
class AFK_AsyncGang
{
protected:
    std::vector<std::thread*> workers;
    std::vector<unsigned int> threadIds;
    AFK_AsyncControls *controls;

    /* The queue of functions and parameters. */
    AFK_WorkQueue<ParameterType, ReturnType, ThreadLocalType> queue;

    /* The thread-local values get put into this known field by the control
     * thread so that the workers can access it without needing a thread
     * restart.
     */
    AFK_ThreadLocalWrapper<ThreadLocalType> threadLocalSource;

    /* The promised return value. */
    std::promise<ReturnType> *promise;

    void initWorkers(void)
    {
        bool first = true;
        for (auto id : threadIds)
        { 
            std::thread *t = new std::thread(
                afk_asyncWorker<ParameterType, ReturnType, ThreadLocalType, taskFinished>,
                std::ref(*controls),
                std::ref(threadLocalSource),
                id,
                first,
                std::ref(queue),
                std::ref(promise));
            workers.push_back(t);
            first = false;
        }
    }

public:
    AFK_AsyncGang(size_t queueSize, AFK_ThreadAllocation& threadAllocation, unsigned int concurrency):
        promise(nullptr)
    {
        /* Work out the actual maximum number of threads I can add
         * to the thread allocation
         */
        unsigned int threadCount = std::min<unsigned int>(concurrency, threadAllocation.getMaxNewIds());

        threadIds.reserve(threadCount);
        for (unsigned int t = 0; t < threadCount; ++t)
            threadIds.push_back(threadAllocation.getNewId());

        controls = new AFK_AsyncControls(threadIds);
        initWorkers();
    }

    AFK_AsyncGang(size_t queueSize, AFK_ThreadAllocation& threadAllocation):
        AFK_AsyncGang(queueSize, threadAllocation, std::thread::hardware_concurrency() + 1)
    {
    }

    virtual ~AFK_AsyncGang()
    {
        controls->control_quit();
        for (auto w : workers) w->join();
        for (auto w : workers) delete w;

        if (promise) delete promise;
        delete controls;
    }

    unsigned int getConcurrency(void) const
    {
        return workers.size();
    }

    const std::vector<unsigned int>& getThreadIds(void) const
    {
        return threadIds;
    }

    bool noQueuedWork(void)
    {
        /* Use as a sanity check.  Makes sure that all current
         * work is finished and we've gated the worker threads
         * out of the inner work loop.
         */
        return taskFinished() && controls->control_workFinished();
    }

    /* Push initial work items into the gang.  Call before calling
     * start().
     * Er, or while it's running, if you feel brave.  It should
     * be fine...
     */
    AFK_AsyncGang& operator<<(typename AFK_WorkQueue<ParameterType, ReturnType, ThreadLocalType>::WorkItem workItem)
    {
        queue.push(workItem);
        return *this;
    }

    /* Tells the async task to do a run.  Returns the future
     * result.
     * The caller should wait on that future, no doubt.
     */
    std::future<ReturnType> start(const ThreadLocalType& _tl)
    {
#if ASYNC_DEBUG_SPAM
        ASYNC_DEBUG("deleting promise " << std::hex << (void *)promise)
#endif
        /* Reset that promise */
        if (promise) delete promise;
        promise = new std::promise<ReturnType>();
#if ASYNC_DEBUG_SPAM
        ASYNC_DEBUG("making new promise " << std::hex << (void *)promise)
#endif

        /* Copy out that thread-local value */
        threadLocalSource.mut.lock();
        threadLocalSource.inner = _tl;
        threadLocalSource.mut.unlock();

        /* Set things off */
        controls->control_workReady();

        /* Send back the future */
        return promise->get_future();
    }
};

#endif /* _AFK_ASYNC_ASYNC_H_ */

