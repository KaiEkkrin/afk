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

#include <cmath>
#include <functional>
#include <future>
#include <iostream>

#include <boost/atomic.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/taus88.hpp>

#include "cache_test.hpp"
#include "map_cache.hpp"
#include "polymer_cache.hpp"
#include "../async/async.hpp"
#include "../clock.hpp"
#include "../rng/hash.hpp"

struct expensivelyHashInt
{
    size_t operator()(const int& v)
    {
#if 0
        boost::random::taus88 rng;
        rng.seed(v);
        return rng() + (((uint64_t)rng()) << 32);
#else
        /* Exercise the hash_swizzle. */
        return static_cast<size_t>(
            afk_hash_swizzle(0ull, static_cast<uint64_t>(v)));
#endif
    }
};

const int afk_cacheTestUnassignedKey = -1;

class IntStartingAtZero
{
public:
    /* For the polymer cache, where the key is included the value. */
    boost::atomic<int> key;

    int v;
    IntStartingAtZero(): key(afk_cacheTestUnassignedKey), v(0) {}
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

        (*(param.cache))[bitcount].v += 1;
        (*(param.cache))[num & 0xff].v += bitcount;
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
    afk_clock::time_point startTime, endTime;
    afk_duration_mfl timeTaken;
    int t;
    std::future<bool> result;

    AFK_ThreadAllocation threadAlloc;
    AFK_AsyncGang<struct insertSqrtParam, bool> gang(
        CACHE_TEST_THREAD_COUNT, threadAlloc, CACHE_TEST_THREAD_COUNT);       

    AFK_WorkQueue<struct insertSqrtParam, bool>::WorkItem items[CACHE_TEST_THREAD_COUNT];
    for (unsigned int i = 0; i < CACHE_TEST_THREAD_COUNT; ++i)
    {
        items[i].func           = testCacheWorker;
        items[i].param.cache    = &mapCache;
        items[i].param.rng      = new boost::random::taus88();
        items[i].param.rng->seed(rdev());
        gang << items[i];
    }

    startTime = afk_clock::now();
    result = gang.start();
    result.wait();
    endTime = afk_clock::now();
    timeTaken = std::chrono::duration_cast<afk_duration_mfl>(endTime - startTime);
    std::cout << "Map cache finished after " << timeTaken.count() << " millis" << std::endl;

    for (int t = 0; t < 32; ++t)
    {
        std::cout << t << " -> " << mapCache[t].v << "; ";
    }
    std::cout << std::endl;

    //std::cout << "MAP CACHE: " << std::endl;
    //mapCache.printEverything(std::cout);
    //std::cout << std::endl;

    /* Now let's try it again with the polymer cache */
    std::function<size_t (const int&)> hashFunc = expensivelyHashInt();
    AFK_PolymerCache<int, IntStartingAtZero, std::function<size_t (const int&)>, afk_cacheTestUnassignedKey, 16> polymerCache(4, hashFunc);

    for (unsigned int i = 0; i < CACHE_TEST_THREAD_COUNT; ++i)
    {
        items[i].param.cache = &polymerCache;
        items[i].param.rng->seed(rdev());
        gang << items[i];
    }

    startTime = afk_clock::now();
    result = gang.start();
    result.wait();
    endTime = afk_clock::now();
    timeTaken = std::chrono::duration_cast<afk_duration_mfl>(endTime - startTime);
    std::cout << "Polymer cache finished after " << timeTaken.count() << " millis" << std::endl;
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

