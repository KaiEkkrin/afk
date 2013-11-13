/* AFK
 * Copyright (C) 2013, Alex Holloway.
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
    ASYNC_CONTROL_DEBUG("control_workReady: " << std::dec << workerCount << " workers")   

    std::unique_lock<std::mutex> lock(workMut);
    while (workReady != 0)
    {
        ASYNC_CONTROL_DEBUG("control_workReady: waiting")

        /* Those workers are busy launching something else */
        workCond.wait(lock);
    }

    workReady = allWorkerMask;
    quit = false;

    ASYNC_CONTROL_DEBUG("control_workReady: (workersBusy " <<
        (workersBusy.is_lock_free() ? "is" : "is not") << " lock free)")

    workCond.notify_all();
}

void AFK_AsyncControls::control_quit(void)
{
    ASYNC_CONTROL_DEBUG("control_quit: sending quit")

    std::unique_lock<std::mutex> lock(workMut);
    while (workReady != 0)
    {
        ASYNC_CONTROL_DEBUG("control_quit: waiting")

        /* Those workers are busy launching something else */
        workCond.wait(lock);
    }

    workReady = allWorkerMask;
    quit = true;
    workCond.notify_all();
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

    /* Wait for everyone else to be working on this task too */
    while (workReady != 0)
    {
        /* Register this thread as working on the task */
        uint64_t busyBit, busyMask;
        afk_workerBusyBitAndMask(id, busyBit, busyMask);
        workReady &= busyMask;
        ASYNC_CONTROL_DEBUG("worker_waitForWork: " << std::hex << workReady << " left over (quit=" << quit << ")")
        workCond.notify_all();

        if (workReady != 0) workCond.wait(lock);
    }

    workCond.notify_all();

    return !quit;
}

