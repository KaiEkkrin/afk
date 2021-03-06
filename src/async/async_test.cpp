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

#include <cassert>
#include <iostream>

#include <boost/atomic.hpp>

#include "async.hpp"
#include "thread_allocation.hpp"
#include "work_queue.hpp"
#include "../clock.hpp"
#include "../file/logstream.hpp"


/* --- async test --- */

/* The shared structure I'll use to record which numbers have
 * been seen
 */
boost::atomic_uint *factors;

/* This structure flags which numbers have been enqueued in
 * the filter
 */
boost::atomic_bool *enqueued;

struct primeFilterParam
{
    unsigned int start;
    unsigned int step;
};

struct primeFilterThreadLocal
{
    unsigned int max;
};

/* To decide when we've finished, this global tracks the number
 * of individual filters currently running.
 */
boost::atomic_uint filtersInFlight;

bool primeFilter(
    unsigned int id,
    const struct primeFilterParam& param,
    const struct primeFilterThreadLocal& threadLocal,
    AFK_WorkQueue<struct primeFilterParam, bool, struct primeFilterThreadLocal>& queue);

/* Helper. */
void enqueueFilter(struct primeFilterParam param, AFK_WorkQueue<struct primeFilterParam, bool, struct primeFilterThreadLocal>& queue)
{
    bool gotIt = false;
    bool isEnqueued = false;

    while (!gotIt)
    {
        gotIt = enqueued[param.start].compare_exchange_weak(isEnqueued, true);
    }

    if (!isEnqueued)
    {
        filtersInFlight.fetch_add(1);

        AFK_WorkQueue<struct primeFilterParam, bool, struct primeFilterThreadLocal>::WorkItem workItem;
        workItem.func = primeFilter;
        workItem.param = param;
        queue.push(workItem);
    }
}

/* I'm going to need an atomically assigned map class.
 * Here would be a good test of its performance, and maybe also that it is
 * working at all:
 * - Make `struct primeFilterParam' keyable
 * - Replace `enqueued' with an atomic map of `struct primeFilterParam' -> `bool'
 * - Replace the loop in this function with a push of the next iteration step
 * (start=start+step; step=step; max=max) onto the queue, using the map to
 * determine whether we tried to queue that one already.
 * It will be interesting to see how well it does compared to this version here
 * (so be sure to keep this original version lying around).
 */
bool primeFilter(
    unsigned int id,
    const struct primeFilterParam& param,
    const struct primeFilterThreadLocal& threadLocal,
    AFK_WorkQueue<struct primeFilterParam, bool, struct primeFilterThreadLocal>& queue)
{
    /* optional verbose debug */
    //afk_out << param.start << "+" << param.step << " ";

    for (unsigned int factor = param.start; factor < threadLocal.max; factor += param.step)
    {
        factors[factor].fetch_add(1);

        /* Go through all the numbers between `factor' and `factor+step' */
        for (unsigned int num = factor + 1; num < (factor + param.step) && num < threadLocal.max; ++num)
        {
            struct primeFilterParam numFilter;
            numFilter.start = num;
            numFilter.step = num;
            enqueueFilter(numFilter, queue);
        }
    }

    /* I finished successfully! */
    filtersInFlight.fetch_sub(1);
    return true;
}

bool filtersFinished(void)
{
    return (filtersInFlight.load() == 0);
}

AFK_AsyncTaskFinishedFunc filtersFinishedFunc = filtersFinished;

void test_pnFilter(unsigned int concurrency, unsigned int primeMax, std::vector<unsigned int>& primes)
{
    afk_clock::time_point startTime, endTime;

    factors = new boost::atomic_uint[primeMax];
    enqueued = new boost::atomic_bool[primeMax];
    for (unsigned int i = 0; i < primeMax; ++i)
    {
        factors[i].store(0);
        enqueued[i].store(false);
    }

    afk_out << "Testing prime number filter with " << concurrency << " threads ..." << std::endl;

    startTime = afk_clock::now();

    {
        /* I'm starting with one filter */
        filtersInFlight.store(1);

        AFK_WorkQueue<struct primeFilterParam, bool, struct primeFilterThreadLocal>::WorkItem i;
        i.func              = primeFilter;
        i.param.start       = 2;
        i.param.step        = 2;

        struct primeFilterThreadLocal tl;
        tl.max = primeMax;

        AFK_ThreadAllocation threadAlloc;

        AFK_AsyncGang<struct primeFilterParam, bool, struct primeFilterThreadLocal, filtersFinishedFunc> primeFilterGang(
            primeMax / 100, threadAlloc, concurrency);
        primeFilterGang << i;
        std::future<bool> finished = primeFilterGang.start(tl); 

        finished.wait();
        afk_out << std::endl << std::endl;
        afk_out << "Finished with " << finished.get() << std::endl;

        /* Obligatory sanity check */
        assert(primeFilterGang.noQueuedWork());
    }

    endTime = afk_clock::now();
    afk_duration_mfl timeTaken = std::chrono::duration_cast<afk_duration_mfl>(endTime - startTime);
    afk_out << concurrency << " threads finished after " << timeTaken.count() << " millis" << std::endl;

    for (unsigned int i = 0; i < primeMax; ++i)
        if (factors[i] == 1)
            primes.push_back(i);

    delete[] factors;
    delete[] enqueued;
}

void check_result(std::vector<unsigned int>& primes1, std::vector<unsigned int>& primes2)
{
    unsigned int i;

    afk_out << "First thirty primes: ";
    for (i = 0; i < 30; ++i)
        afk_out << primes2[i] << " ";
    afk_out << std::endl;

    if (primes1.size() != primes2.size())
    {
        afk_out << "LISTS DIFFER IN SIZE: " << primes1.size() << " vs " << primes2.size() << std::endl;
    }
    else
    {
        for (i = 0; i < primes1.size(); ++i)
        {
            if (primes1[i] != primes2[i])
            {
                afk_out << "ENTRIES AT " << i << " DIFFER: " << primes1[i] << " vs " << primes2[i] << std::endl;
                break;
            }
        }

        if (i == primes1.size())
            afk_out << "Lists are the same" << std::endl;
    }  
}

void test_async(void)
{
    std::vector<unsigned int> primes[6];
    unsigned int primeMax = 50000;

    test_pnFilter(1, primeMax, primes[0]);
    afk_out << std::endl;

    test_pnFilter(2, primeMax, primes[1]);
    check_result(primes[0], primes[1]);
    afk_out << std::endl;

    test_pnFilter(4, primeMax, primes[2]);
    check_result(primes[0], primes[2]);
    afk_out << std::endl;

    test_pnFilter(std::thread::hardware_concurrency(), primeMax, primes[3]);
    check_result(primes[0], primes[3]);
    afk_out << std::endl;

    test_pnFilter(std::thread::hardware_concurrency() * 2, primeMax, primes[4]);
    check_result(primes[0], primes[4]);
    afk_out << std::endl;

    test_pnFilter(std::thread::hardware_concurrency() * 4, primeMax, primes[5]);
    check_result(primes[0], primes[5]);
    afk_out << std::endl;
}

