/* AFK (c) Alex Holloway 2013 */

#include "async.hpp"

/* Don't enable this unless you want MAXIMAL SPAM */
#define ASYNC_DEBUG_SPAM 0

#if ASYNC_DEBUG_SPAM
#include <iostream>
#include <boost/date_time/posix_time/posix_time.hpp>

boost::mutex debugSpamMut;

#define ASYNC_DEBUG(chain) \
    { \
        boost::unique_lock<boost::mutex> guard(debugSpamMut); \
        boost::posix_time::ptime debugTime = boost::posix_time::microsec_clock::local_time(); \
        std::cout << "control " << this << " in thread "; \
        std::cout << boost::this_thread::get_id() << " at " << debugTime << ": "; \
        std::cout << chain << std::endl; \
    }

#else
#define ASYNC_DEBUG(chain)
#endif /* ASYNC_DEBUG_SPAM */

void AFK_AsyncControls::control_workReady(unsigned int workerCount)
{
    ASYNC_DEBUG("control_workReady: " << workerCount << " workers")   

    boost::unique_lock<boost::mutex> lock(workMut);
    while (workReady != 0)
    {
        ASYNC_DEBUG("control_workReady: waiting")

        /* Those workers are busy launching something else */
        workCond.wait(lock);
    }

    workReady = workerCount;
    quit = false;
    workersBusy.store(workerCount);

    ASYNC_DEBUG("control_workReady: (workersBusy " <<
        (workersBusy.is_lock_free() ? "is" : "is not") << " lock free)")

    workCond.notify_all();
}

void AFK_AsyncControls::control_quit(void)
{
    ASYNC_DEBUG("control_quit: sending quit")

    boost::unique_lock<boost::mutex> lock(workMut);
    while (workReady != 0)
    {
        ASYNC_DEBUG("control_quit: waiting")

        /* Those workers are busy launching something else */
        workCond.wait(lock);
    }

    quit = true;
    workCond.notify_all();
}

bool AFK_AsyncControls::worker_waitForWork(void)
{
    ASYNC_DEBUG("worker_waitForWork: entry")

    boost::unique_lock<boost::mutex> lock(workMut);
    while (workReady == 0 && !quit)
    {
        ASYNC_DEBUG("worker_waitForWork: waiting")

        /* There's no work, wait for some to turn up. */
        workCond.wait(lock);
    }

    /* Register this thread as working on the task */
    --workReady;
    ASYNC_DEBUG("worker_waitForWork: " << workReady << " left over (quit=" << quit << ")")
    return !quit;
}

void AFK_AsyncControls::worker_amBusy(void)
{
    workersBusy.fetch_add(1);
    ASYNC_DEBUG("worker_amBusy: now " << workersBusy << " busy")
}

void AFK_AsyncControls::worker_amIdle(void)
{
    workersBusy.fetch_sub(1);
    ASYNC_DEBUG("worker_amIdle: now " << workersBusy << " busy")
}

bool AFK_AsyncControls::worker_allIdle(void)
{
    /* If I get 1 back, I just decremented it to zero, meaning
     * there are no busy workers left
     */
    return (workersBusy == 0);
}

