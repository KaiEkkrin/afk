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

std::ostream& operator<<(std::ostream& os, const IntStartingAtZero& i)
{
    return os << i.v;
}

struct insertSqrtParam
{
    AFK_Cache<int, IntStartingAtZero> *cache;
    boost::random::taus88 *rng;
};

#define CACHE_TEST_ITERATIONS 400000

bool testCacheWorker(unsigned int id, const struct insertSqrtParam& param, AFK_WorkQueue<struct insertSqrtParam, bool>& queue)
{
    for (unsigned int i = 0; i < CACHE_TEST_ITERATIONS; ++i)
    {
        int num = (*(param.rng))();

        /* Take the bit count of this number */
        int bitcount = 0;
        for (unsigned int biti = 0; biti < 32; ++biti)
            if (num & (1 << biti)) ++bitcount;

        (*(param.cache))[bitcount] += 1;
        (*(param.cache))[num % 0xff] += bitcount;
    }

    return true;
}

#define CACHE_TEST_THREAD_COUNT 16

/* I'm not testing eviction here right now, but I need to provide
 * a function anyway.
 */
bool testCache_canEvict(const IntStartingAtZero& value)
{
    return false;
}

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
        CACHE_TEST_THREAD_COUNT, CACHE_TEST_THREAD_COUNT);       

    AFK_WorkQueue<struct insertSqrtParam, bool>::WorkItem items[CACHE_TEST_THREAD_COUNT];
    for (unsigned int i = 0; i < CACHE_TEST_THREAD_COUNT; ++i)
    {
        items[i].func           = testCacheWorker;
        items[i].param.cache    = &mapCache;
        items[i].param.rng      = new boost::random::taus88();
        items[i].param.rng->seed(rdev());
        gang << items[i];
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

    //std::cout << "MAP CACHE: " << std::endl;
    //mapCache.printEverything(std::cout);
    //std::cout << std::endl;

    /* Now let's try it again with the polymer cache */
    boost::function<size_t (const int&)> hashFunc = expensivelyHashInt();
    AFK_PolymerCache<int, IntStartingAtZero, boost::function<size_t (const int&)> > polymerCache(16, 4, hashFunc);

    for (unsigned int i = 0; i < CACHE_TEST_THREAD_COUNT; ++i)
    {
        items[i].param.cache = &polymerCache;
        items[i].param.rng->seed(rdev());
        gang << items[i];
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

    for (unsigned int i = 0; i < CACHE_TEST_THREAD_COUNT; ++i)
    {
        delete items[i].param.rng;
    }

    //std::cout << "POLYMER CACHE: " << std::endl;
    //polymerCache.printEverything(std::cout);
    //std::cout << std::endl;
}

