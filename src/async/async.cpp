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

#include "async.hpp"

#include <cassert>

#if ASYNC_DEBUG_SPAM
std::mutex debugSpamMut;
#endif

static void afk_workerBusyBitAndMask(unsigned int id, uint64_t& o_busyBit, uint64_t& o_busyMask)
{
    o_busyBit = 1ull << id;
    o_busyMask = ~o_busyBit;
}

void AFK_AsyncControls::control_workReady(void)
{
    ASYNC_CONTROL_DEBUG("control_workReady: " << std::hex << allWorkerMask << " worker mask")   

    std::unique_lock<std::mutex> lock(workMut);
    while ((workReady != 0 || workRunning != 0) && !quit)
    {
        ASYNC_CONTROL_DEBUG("control_workReady: waiting")

        /* Those workers are busy launching something else */
        workCond.wait(lock);
    }

    workReady = allWorkerMask;
    assert(workRunning == 0);
    quit = false;

    workCond.notify_all();
}

void AFK_AsyncControls::control_quit(void)
{
    ASYNC_CONTROL_DEBUG("control_quit: sending quit")

    std::unique_lock<std::mutex> lock(workMut);

    /* I should be able to just flag for quit right away. */
    quit = true;
    workCond.notify_all();
    ASYNC_CONTROL_DEBUG("control_quit: flagged")
}

bool AFK_AsyncControls::worker_waitForWork(unsigned int id)
{
    ASYNC_CONTROL_DEBUG("worker_waitForWork: entry")

    std::unique_lock<std::mutex> lock(workMut);
    while (workReady == 0 && !quit)
    {
        ASYNC_CONTROL_DEBUG("worker_waitForWork: waiting")

        /* There's no work, wait for some to turn up. */
        workCond.wait(lock);
    }
    
    ASYNC_CONTROL_DEBUG("worker_waitForWork: syncing with other workers")

    /* Register this thread as working on the task */
    uint64_t busyBit, busyMask;
    afk_workerBusyBitAndMask(id, busyBit, busyMask);
    workReady &= busyMask;
    workRunning |= busyBit;
    workCond.notify_all();

    /* Wait for everyone else to be working on this task too. */
    while (workReady != 0 && !quit)
    {
        ASYNC_CONTROL_DEBUG("worker_waitForWork: " << std::hex << workReady << " left over (quit=" << quit << ")")
        workCond.wait(lock);
    }

    return !quit;
}

void AFK_AsyncControls::worker_waitForFinished(unsigned int id)
{
    ASYNC_CONTROL_DEBUG("worker_waitForFinished: entry")

    std::unique_lock<std::mutex> lock(workMut);

    assert(workReady == 0); /* TODO, see above, I feel I ought not to need this */

    /* Clear my running bit first... */
    uint64_t busyBit, busyMask;
    afk_workerBusyBitAndMask(id, busyBit, busyMask);
    workRunning &= busyMask;

    /* ... and now, wait for all the others.
     * In this loop, we need to remember to check for
     * work being ready too; if our thread is delayed at this point,
     * the control thread may have found no work ready or running and
     * added more, and a different thread may have grabbed some and
     * declared itself running already, at which point we need to
     * hop along and grab it too.  Everyone will wait before re-entry
     * to the work loop. (in worker_waitForWork() ).
     */
    while (workReady == 0 && workRunning != 0 && !quit)
    {
        ASYNC_CONTROL_DEBUG("worker_waitForFinished: " << std::hex << workRunning << " still running (quit=" << quit << ")")
        workCond.wait(lock);
    }

    /* This is needed to wake up the control thread and tell it it can
     * ready the next batch
     */
    workCond.notify_all();
}

bool AFK_AsyncControls::control_workFinished(void)
{
    std::unique_lock<std::mutex> lock(workMut);
    return (workRunning == 0);
}

