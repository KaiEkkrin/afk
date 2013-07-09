/* AFK (c) Alex Holloway 2013 */

#include "async.hpp"

#if ASYNC_DEBUG_SPAM
boost::mutex debugSpamMut;
#endif

void afk_workerBusyBitAndMask(unsigned int id, size_t& o_busyBit, size_t& o_busyMask)
{
    o_busyBit = (size_t)1 << id;
    o_busyMask = ~o_busyBit;
}

void AFK_AsyncControls::control_workReady(void)
{
    ASYNC_CONTROL_DEBUG("control_workReady: " << std::dec << workerCount << " workers")   

    boost::unique_lock<boost::mutex> lock(workMut);
    while (workReady != 0)
    {
        ASYNC_CONTROL_DEBUG("control_workReady: waiting")

        /* Those workers are busy launching something else */
        workCond.wait(lock);
    }

    workReady = ((size_t)1 << workerCount) - 1;
    quit = false;

    ASYNC_CONTROL_DEBUG("control_workReady: (workersBusy " <<
        (workersBusy.is_lock_free() ? "is" : "is not") << " lock free)")

    workCond.notify_all();
}

void AFK_AsyncControls::control_quit(void)
{
    ASYNC_CONTROL_DEBUG("control_quit: sending quit")

    boost::unique_lock<boost::mutex> lock(workMut);
    while (workReady != 0)
    {
        ASYNC_CONTROL_DEBUG("control_quit: waiting")

        /* Those workers are busy launching something else */
        workCond.wait(lock);
    }

    workReady = ((size_t)1 << workerCount) - 1;
    quit = true;
    workCond.notify_all();
}

bool AFK_AsyncControls::worker_waitForWork(unsigned int id)
{
    ASYNC_CONTROL_DEBUG("worker_waitForWork: entry")

    boost::unique_lock<boost::mutex> lock(workMut);
    while (workReady == 0 && !quit)
    {
        ASYNC_CONTROL_DEBUG("worker_waitForWork: waiting")

        /* There's no work, wait for some to turn up. */
        workCond.wait(lock);
    }
    
    ASYNC_CONTROL_DEBUG("worker_waitForWork: syncing with other workers")

    /* Wait for everyone else to be working on this task too */
    while (workReady != 0)
    {
        /* Register this thread as working on the task */
        size_t busyBit, busyMask;
        afk_workerBusyBitAndMask(id, busyBit, busyMask);
        workReady &= busyMask;
        ASYNC_CONTROL_DEBUG("worker_waitForWork: " << std::hex << workReady << " left over (quit=" << quit << ")")
        workCond.notify_all();

        workCond.wait(lock);
    }

    workCond.notify_all();

    return !quit;
}

