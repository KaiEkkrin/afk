/* AFK (c) Alex Holloway 2013 */

#include "async.hpp"

void AFK_AsyncControls::control_workReady(unsigned int workerCount)
{
    boost::unique_lock<boost::mutex> lock(workMut);
    while (workReady != 0)
    {
        /* Those workers are busy launching something else */
        workCond.wait(lock);
    }

    workReady = workerCount;
    quit = false;
    workersBusy.store(workerCount);
}

void AFK_AsyncControls::control_quit(void)
{
    boost::unique_lock<boost::mutex> lock(workMut);
    while (workReady != 0)
    {
        /* Those workers are busy launching something else */
        workCond.wait(lock);
    }

    workReady = 42; /* doesn't matter, but should be nonzero */
    quit = true;
}

bool AFK_AsyncControls::worker_waitForWork(void)
{
    boost::unique_lock<boost::mutex> lock(workMut);
    while (workReady == 0)
    {
        /* There's no work, wait for some to turn up. */
        workCond.wait(lock);
    }

    /* Register this thread as working on the task */
    --workReady;
    return quit;
}

void AFK_AsyncControls::worker_amBusy(void)
{
    workersBusy.fetch_add(1);
}

bool AFK_AsyncControls::worker_amIdle(void)
{
    /* If I get 1 back, I just decremented it to zero, meaning
     * there are no busy workers left
     */
    return workersBusy.fetch_sub(1) == 1;
}

