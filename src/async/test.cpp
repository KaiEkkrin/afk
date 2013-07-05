/* AFK (c) Alex Holloway 2013 */

#include <iostream>

/* TODO Is the boost thread stuff incompatible with std::vector ?? */
//#include <vector>

#include <boost/date_time/posix_time/posix_time.hpp>

#include "async.hpp"

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

/* Helper. */
void enqueueFilter(struct primeFilterParam param, ASYNC_QUEUE_TYPE(struct primeFilterParam)& queue)
{
    bool gotIt = false;
    bool isEnqueued = false;

    while (!gotIt)
    {
        gotIt = enqueued[param.start].compare_exchange_weak(isEnqueued, true);
    }

    if (!isEnqueued)
    {
        queue.push(param);
    }
}

bool primeFilter(const struct primeFilterParam& param, ASYNC_QUEUE_TYPE(struct primeFilterParam)& queue)
{
    /* `start + step' should be flagged as a factor */
    factors[param.start + param.step].fetch_add(1);

    /* Go through all the numbers between `start' and `step' */
    for (unsigned int num = param.start + 1; num < param.step && num < param.max; ++num)
    {
        /* If it's flagged as 0 ... */
        if (factors[num].fetch_add(0) == 0)
        {
            /* ...and if it hasn't yet been enqueued into the filter ... */
            struct primeFilterParam numFilter;
            numFilter.start = num;
            numFilter.step = num;
            numFilter.max = param.max;
            enqueueFilter(numFilter, queue);
        }
    }

    /* Enqueue the next step */
    struct primeFilterParam nextStep;
    nextStep.start = param.start + param.step;
    nextStep.step = param.step;
    nextStep.max = param.max;
    enqueueFilter(nextStep, queue);

    /* I finished successfully! */
    return true;
}

void test_pnFilter(unsigned int concurrency, unsigned int primeMax)
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
        struct primeFilterParam p;
        p.start = 2;
        p.max = primeMax;

        AFK_AsyncGang<struct primeFilterParam, bool> primeFilterGang(
            boost::function<bool (const struct primeFilterParam&, ASYNC_QUEUE_TYPE(struct primeFilterParam)&)>(primeFilter),
            primeMax / 100, concurrency);
        primeFilterGang << p;
        boost::unique_future<bool> finished = primeFilterGang.start(); 

        /* TODO Do something more clever with this.  I need to
         * think hard about the manner in which the caller is
         * woken up when the result has been computed.
         */
        finished.wait();
        std::cout << "Finished with " << finished.get();
    }

    endTime = boost::posix_time::microsec_clock::local_time();
    std::cout << "1 thread finished after " << endTime - startTime << std::endl;

    delete[] factors;
    delete[] enqueued;
}

void test_async(void)
{
    unsigned int primeMax = 1000000;

    test_pnFilter(1, primeMax);
    std::cout << std::endl;
    test_pnFilter(2, primeMax);
    std::cout << std::endl;
    test_pnFilter(boost::thread::hardware_concurrency(), primeMax);
    std::cout << std::endl;
}

