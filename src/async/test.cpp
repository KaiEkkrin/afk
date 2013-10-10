/* AFK (c) Alex Holloway 2013 */

#include <iostream>

#include <boost/date_time/posix_time/posix_time.hpp>

#include "async.hpp"
#include "work_queue.hpp"


/* --- async test --- */

/* The shared structure I'll use to record which numbers have
 * been seen
 */
boost::atomic<unsigned int> *factors;

/* This structure flags which numbers have been enqueued in
 * the filter
 */
boost::atomic<bool> *enqueued;

struct primeFilterParam
{
    unsigned int start;
    unsigned int step;
    unsigned int max;
};

bool primeFilter(unsigned int id, const struct primeFilterParam& param, AFK_WorkQueue<struct primeFilterParam, bool>& queue);

/* Helper. */
void enqueueFilter(struct primeFilterParam param, AFK_WorkQueue<struct primeFilterParam, bool>& queue)
{
    bool gotIt = false;
    bool isEnqueued = false;

    while (!gotIt)
    {
        gotIt = enqueued[param.start].compare_exchange_weak(isEnqueued, true);
    }

    if (!isEnqueued)
    {
        AFK_WorkQueue<struct primeFilterParam, bool>::WorkItem workItem;
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
bool primeFilter(unsigned int id, const struct primeFilterParam& param, AFK_WorkQueue<struct primeFilterParam, bool>& queue)
{
    /* optional verbose debug */
    //std::cout << param.start << "+" << param.step << " ";

    for (unsigned int factor = param.start; factor < param.max; factor += param.step)
    {
        factors[factor].fetch_add(1);

        /* Go through all the numbers between `factor' and `factor+step' */
        for (unsigned int num = factor + 1; num < (factor + param.step) && num < param.max; ++num)
        {
            struct primeFilterParam numFilter;
            numFilter.start = num;
            numFilter.step = num;
            numFilter.max = param.max;
            enqueueFilter(numFilter, queue);
        }
    }

    /* I finished successfully! */
    return true;
}

void test_pnFilter(unsigned int concurrency, unsigned int primeMax, std::vector<unsigned int>& primes)
{
    boost::posix_time::ptime startTime, endTime;

    factors = new boost::atomic<unsigned int>[primeMax];
    enqueued = new boost::atomic<bool>[primeMax];
    for (unsigned int i = 0; i < primeMax; ++i)
    {
        factors[i].store(0);
        enqueued[i].store(false);
    }

    std::cout << "Testing prime number filter with " << concurrency << " threads ..." << std::endl;

    startTime = boost::posix_time::microsec_clock::local_time();

    {
        AFK_WorkQueue<struct primeFilterParam, bool>::WorkItem i;
        i.func              = primeFilter;
        i.param.start       = 2;
        i.param.step        = 2;
        i.param.max         = primeMax;

        AFK_AsyncGang<struct primeFilterParam, bool> primeFilterGang(
            primeMax / 100, concurrency);
        primeFilterGang << i;
        boost::unique_future<bool> finished = primeFilterGang.start(); 

        finished.wait();
        std::cout << std::endl << std::endl;
        std::cout << "Finished with " << finished.get() << std::endl;
    }

    endTime = boost::posix_time::microsec_clock::local_time();
    std::cout << concurrency << " threads finished after " << endTime - startTime << std::endl;

    for (unsigned int i = 0; i < primeMax; ++i)
        if (factors[i] == 1)
            primes.push_back(i);

    delete[] factors;
    delete[] enqueued;
}

void check_result(std::vector<unsigned int>& primes1, std::vector<unsigned int>& primes2)
{
    unsigned int i;

    std::cout << "First thirty primes: ";
    for (i = 0; i < 30; ++i)
        std::cout << primes2[i] << " ";
    std::cout << std::endl;

    if (primes1.size() != primes2.size())
    {
        std::cout << "LISTS DIFFER IN SIZE: " << primes1.size() << " vs " << primes2.size() << std::endl;
    }
    else
    {
        for (i = 0; i < primes1.size(); ++i)
        {
            if (primes1[i] != primes2[i])
            {
                std::cout << "ENTRIES AT " << i << " DIFFER: " << primes1[i] << " vs " << primes2[i] << std::endl;
                break;
            }
        }

        if (i == primes1.size())
            std::cout << "Lists are the same" << std::endl;
    }  
}

void test_async(void)
{
    std::vector<unsigned int> primes[6];
    unsigned int primeMax = 50000;

    test_pnFilter(1, primeMax, primes[0]);
    std::cout << std::endl;

    test_pnFilter(2, primeMax, primes[1]);
    check_result(primes[0], primes[1]);
    std::cout << std::endl;

    test_pnFilter(4, primeMax, primes[2]);
    check_result(primes[0], primes[2]);
    std::cout << std::endl;

    test_pnFilter(boost::thread::hardware_concurrency(), primeMax, primes[3]);
    check_result(primes[0], primes[3]);
    std::cout << std::endl;

    test_pnFilter(boost::thread::hardware_concurrency() * 2, primeMax, primes[4]);
    check_result(primes[0], primes[4]);
    std::cout << std::endl;

    test_pnFilter(boost::thread::hardware_concurrency() * 4, primeMax, primes[5]);
    check_result(primes[0], primes[5]);
    std::cout << std::endl;
}

