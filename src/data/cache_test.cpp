/* AFK (c) Alex Holloway 2013 */

#include <cmath>
#include <iostream>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/function.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/taus88.hpp>
#include <boost/thread/future.hpp>

#include "cache_test.hpp"
#include "map_cache.hpp"
#include "polymer_cache.hpp"
#include "../async/async.hpp"

struct expensivelyHashInt
{
    size_t operator()(const int& v)
    {
        boost::random::taus88 rng;
        rng.seed(v);
        return rng() + (((unsigned long long)rng()) << 32);
    }
};

class IntStartingAtZero
{
public:
    int v;
    IntStartingAtZero(): v(0) {}
    IntStartingAtZero& operator+=(const int& o) { v += o; return *this; }
};

struct insertSqrtParam
{
    AFK_Cache<int, IntStartingAtZero> *cache;
    boost::random::taus88 rng;
};

#define CACHE_TEST_ITERATIONS 400000

bool testCacheWorker(struct insertSqrtParam param, ASYNC_QUEUE_TYPE(struct insertSqrtParam)& queue)
{
    for (unsigned int i = 0; i < CACHE_TEST_ITERATIONS; ++i)
    {
        int num = param.rng();

        /* Take the bit count of this number */
        int bitcount = 0;
        for (unsigned int biti = 0; biti < 32; ++biti)
            if (num & (1 << biti)) ++bitcount;

        (*(param.cache))[bitcount] += 1;
    }

    return true;
}

#define CACHE_TEST_THREAD_COUNT 16

void test_cache(void)
{
    /* Really simple -- just insert the square roots of a lot of
     * random numbers and count how many times they occurred
     */
    AFK_MapCache<int, IntStartingAtZero> mapCache;
    boost::random::random_device rdev;
    boost::posix_time::ptime startTime, endTime;
    int t;
    boost::unique_future<bool> result;

    AFK_AsyncGang<struct insertSqrtParam, bool> gang(
        testCacheWorker, CACHE_TEST_THREAD_COUNT, CACHE_TEST_THREAD_COUNT);       

    struct insertSqrtParam params[CACHE_TEST_THREAD_COUNT];
#if 0
    for (unsigned int i = 0; i < CACHE_TEST_THREAD_COUNT; ++i)
    {
        params[i].cache = &mapCache;
        params[i].rng.seed(rdev());
        gang << params[i];
    }

    startTime = boost::posix_time::microsec_clock::local_time();
    result = gang.start();
    result.wait();
    endTime = boost::posix_time::microsec_clock::local_time();
    std::cout << "Map cache finished after " << endTime - startTime << std::endl;

    for (int t = 0; t < 32; ++t)
    {
        std::cout << t << " -> " << mapCache[t].v << "; ";
    }
    std::cout << std::endl;
#endif

    /* Now let's try it again with the polymer cache */
    boost::function<size_t (const int&)> hashFunc = expensivelyHashInt();
    AFK_PolymerCache<int, IntStartingAtZero, boost::function<size_t (const int&)> > polymerCache(hashFunc);

    for (unsigned int i = 0; i < CACHE_TEST_THREAD_COUNT; ++i)
    {
        params[i].cache = &polymerCache;
        params[i].rng.seed(rdev());
        gang << params[i];
    }

    startTime = boost::posix_time::microsec_clock::local_time();
    result = gang.start();
    result.wait();
    endTime = boost::posix_time::microsec_clock::local_time();
    std::cout << "Polymer cache finished after " << endTime - startTime << std::endl;
    polymerCache.printStats(std::cout, "Polymer stats");

    for (t = 0; t < 32; ++t)
    {
        std::cout << t << " -> " << polymerCache[t].v << "; ";
    }
    std::cout << std::endl;


}

