/* AFK (c) Alex Holloway 2013 */

#include <iostream>

#include <boost/date_time/posix_time/posix_time.hpp>

#include "async.hpp"
#include "substrate.hpp"


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

/* TODO: I'm going to need an atomically assigned map class.
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
bool primeFilter(const struct primeFilterParam& param, ASYNC_QUEUE_TYPE(struct primeFilterParam)& queue)
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
        struct primeFilterParam p;
        p.start = 2;
        p.step = 2;
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


/* --- substrate test --- */


struct STS
{
    float f;
    STS(): f(0.0f) {}
};

void test_substrate(void)
{
    boost::posix_time::ptime startTime, endTime;

    /* first with new (to check), then with a substrate. */
#define SUBTEST_CHUNK_SIZE 100000

    unsigned int i;
    bool ok;

    struct STS **newF1 = new struct STS*[SUBTEST_CHUNK_SIZE];
    struct STS **newF2 = new struct STS*[SUBTEST_CHUNK_SIZE];

    startTime = boost::posix_time::microsec_clock::local_time();

    /* allocate many floats */
    for (i = 0; i < SUBTEST_CHUNK_SIZE; ++i)
        newF1[i] = new struct STS();

    /* assign them a value equal to their index squared */
    for (i = 0; i < SUBTEST_CHUNK_SIZE; ++i)
        newF1[i]->f = ((float)i * (float)i);

    /* allocate another batch */
    for (i = 0; i < SUBTEST_CHUNK_SIZE; ++i)
        newF2[i] = new struct STS();

    /* assign them a value equal to their index cubed */
    for (i = 0; i < SUBTEST_CHUNK_SIZE; ++i)
        newF2[i]->f = ((float)i * (float)i * (float)i);

    /* delete half the first batch */
    for (i = 0; i < (SUBTEST_CHUNK_SIZE / 2); ++i)
        delete newF1[i];

    /* add 1 to the rest */
    for (i = (SUBTEST_CHUNK_SIZE / 2); i < SUBTEST_CHUNK_SIZE; ++i)
        newF1[i]->f += 1;

    for (i = 0; i < SUBTEST_CHUNK_SIZE; ++i)
        newF2[i]->f += 1;

    /* delete half the second batch */
    for (i = 0; i < (SUBTEST_CHUNK_SIZE / 2); ++i)
        delete newF2[i];

    /* re-allocate the first half of the first batch and assign
     * them all the square minus one
     */
    for (i = 0; i < (SUBTEST_CHUNK_SIZE / 2); ++i)
    {
        newF1[i] = new struct STS();
        newF1[i]->f = ((float)i * (float)i) - 1.0f;
    }

    /* go through it all.  the first half of newF1 should be i**2-1.
     * the second half should be i**2+1.  the second half of newF2
     * should be i**3+1
     */
    ok = true;
    for (i = 0; i < SUBTEST_CHUNK_SIZE; ++i)
    {
        if (i < (SUBTEST_CHUNK_SIZE / 2))
        {
            if (newF1[i]->f != ((float)i * (float)i - 1))
            {
                std::cout << "x";
                ok = false;
            }
        }
        else
        {
            if (newF1[i]->f != ((float)i * (float)i + 1))
            {
                std::cout << "x";
                ok = false;
            }

            if (newF2[i]->f != ((float)i * (float)i * (float)i + 1))
            {
                std::cout << "x";
                ok = false;
            }
        }
    }

    /* clean up */
    for (i = 0; i < SUBTEST_CHUNK_SIZE; ++i)
        delete newF1[i];

    for (i = (SUBTEST_CHUNK_SIZE / 2); i < SUBTEST_CHUNK_SIZE; ++i)
        delete newF2[i];

    endTime = boost::posix_time::microsec_clock::local_time();

    delete[] newF1;
    delete[] newF2;

    std::cout << "single thread new test: finished in " << endTime - startTime << std::endl;
    if (ok) std::cout << "no errors" << std::endl;

    /* now do the same thing with a substrate (this will so break) */
    AFK_Substrate<struct STS> sub1(16, 12);
    size_t *newF1S = new size_t[SUBTEST_CHUNK_SIZE];
    size_t *newF2S = new size_t[SUBTEST_CHUNK_SIZE];
    startTime = boost::posix_time::microsec_clock::local_time();

    /* allocate many floats */
    for (i = 0; i < SUBTEST_CHUNK_SIZE; ++i)
        newF1S[i] = sub1.alloc();

    /* assign them a value equal to their index squared */
    for (i = 0; i < SUBTEST_CHUNK_SIZE; ++i)
        sub1[newF1S[i]].f = ((float)i * (float)i);

    /* allocate another batch */
    for (i = 0; i < SUBTEST_CHUNK_SIZE; ++i)
        newF2S[i] = sub1.alloc();

    /* assign them a value equal to their index cubed */
    for (i = 0; i < SUBTEST_CHUNK_SIZE; ++i)
        sub1[newF2S[i]].f = ((float)i * (float)i * (float)i);

    /* delete half the first batch */
    for (i = 0; i < (SUBTEST_CHUNK_SIZE / 2); ++i)
        sub1.free(newF1S[i]);

    /* add 1 to the rest */
    for (i = (SUBTEST_CHUNK_SIZE / 2); i < SUBTEST_CHUNK_SIZE; ++i)
        sub1[newF1S[i]].f += 1;

    for (i = 0; i < SUBTEST_CHUNK_SIZE; ++i)
        sub1[newF2S[i]].f += 1;

    /* delete half the second batch */
    for (i = 0; i < (SUBTEST_CHUNK_SIZE / 2); ++i)
        sub1.free(newF2S[i]);

    /* re-allocate the first half of the first batch and assign
     * them all the square minus one
     */
    for (i = 0; i < (SUBTEST_CHUNK_SIZE / 2); ++i)
    {
        newF1S[i] = sub1.alloc();
        sub1[newF1S[i]].f = ((float)i * (float)i) - 1.0f;
    }

    /* go through it all.  the first half of newF1 should be i**2-1.
     * the second half should be i**2+1.  the second half of newF2
     * should be i**3+1
     */
    ok = true;
    for (i = 0; i < SUBTEST_CHUNK_SIZE; ++i)
    {
        if (i < (SUBTEST_CHUNK_SIZE / 2))
        {
            if (sub1[newF1S[i]].f != ((float)i * (float)i - 1))
            {
                std::cout << "x";
                ok = false;
            }
        }
        else
        {
            if (sub1[newF1S[i]].f != ((float)i * (float)i + 1))
            {
                std::cout << "x";
                ok = false;
            }

            if (sub1[newF2S[i]].f != ((float)i * (float)i * (float)i + 1))
            {
                std::cout << "x";
                ok = false;
            }
        }
    }

    /* clean up */
    for (i = 0; i < SUBTEST_CHUNK_SIZE; ++i)
        sub1.free(newF1S[i]);

    for (i = (SUBTEST_CHUNK_SIZE / 2); i < SUBTEST_CHUNK_SIZE; ++i)
        sub1.free(newF2S[i]);

    delete[] newF1S;
    delete[] newF2S;

    endTime = boost::posix_time::microsec_clock::local_time();

    std::cout << "single thread substrate test: finished in " << endTime - startTime << std::endl;
    if (ok) std::cout << "no errors" << std::endl;
    sub1.printStats(std::cout, "sub1");
}

