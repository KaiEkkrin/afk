/* AFK (c) Alex Holloway 2013 */

#include <functional>
#include <iostream>
#include <list>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/random/random_device.hpp>

#include <openssl/engine.h>

#include "aes.hpp"
#include "boost_mt19937.hpp"
#include "boost_taus88.hpp"
#include "c.hpp"
#include "hmac.hpp"
#include "test.hpp"
#include "../afk.hpp"
#include "../cell.hpp"


/* TODO Add a test that tweaks each coord in an AFK_Cell in
 * turn, and checks that the RNG output changes entirely.
 * (Important for RNGs composed out of several smaller ones,
 * like the taus88 implementation.)
 */


static void evaluate_rng(AFK_RNG& rng, const std::string& name, const AFK_Cell* testCells, const size_t testCellCount, const size_t randsPerCell)
{
    AFK_RNG_Test rng_test(&rng, 20, 1000);

    std::cout << "Testing RNG: " << name << std::endl;

    /* The first 10 test cells are used for the contribution test. */   
    unsigned int i;
    for (i = 0; i < 10; ++i)
        rng_test.contribute(testCells[i].rngSeed());

    rng_test.printResults();

    /* now do a performance test.  How long does it take
     * to fill many many floats ?
     */

    std::cout << "Performance test: trying " << (testCellCount / 2) - i << " cells taking " << randsPerCell << " randoms each" << std::endl;

    float smallestValue = 1.0f, largestValue = 0.0f;
    float *manyFloats = new float[randsPerCell];
    float *sampleFloats = new float[testCellCount];

    boost::posix_time::ptime ptestStartTime = boost::posix_time::microsec_clock::local_time();
    for (; i < testCellCount / 2; ++i)
    {
        rng.seed(testCells[i].rngSeed());
        for (unsigned int j = 0; j < randsPerCell; ++j)
            manyFloats[j] = rng.frand();

        sampleFloats[i] = manyFloats[rng.rand().v.ui[0] % randsPerCell];
    }
    float sampleFloat = sampleFloats[rng.rand().v.ui[0] % testCellCount / 2];
    boost::posix_time::ptime ptestEndTime = boost::posix_time::microsec_clock::local_time();

    std::cout << "Test took " << (ptestEndTime - ptestStartTime) << " with " << name << std::endl;
    std::cout << "Smallest value: " << smallestValue << std::endl;
    std::cout << "Largest value: " << largestValue << std::endl;
    std::cout << "Random float from the set: " << sampleFloat << std::endl;

    delete[] sampleFloats;
    delete[] manyFloats;

    /* Lastly, test for clashes in the individual result bytes
     * of the first result.
     * If there are few values hit often, my RNG's first output is
     * probably a good hash function.
     * It's also instructive to see how quick it is at
     * producing this output.
     */
    unsigned int hits[4][256];
    memset(hits, 0, 4 * 256 * sizeof(unsigned int));

    ptestStartTime = boost::posix_time::microsec_clock::local_time();
    for (; i < testCellCount; ++i)
    {
        rng.seed(testCells[i].rngSeed());
        AFK_RNG_Value val = rng.rand();

        for (int j = 0; j < 16; j += 4)
            for (int k = 0; k < 4; ++k)
                ++hits[k][val.v.b[j+k]];
    }
    ptestEndTime = boost::posix_time::microsec_clock::local_time();
    std::cout << "Hit count test took " << (ptestEndTime - ptestStartTime) << " with " << name << std::endl;

    /* Do the analysis. */
    for (int j = 0; j < 4; ++j)
    {
        std::list<unsigned int> sortedHitCounts;

        for (int k = 0; k < 256; ++k)
            sortedHitCounts.push_back(hits[j][k]);

        sortedHitCounts.sort(std::greater<unsigned int>());

        std::cout << "Hit counts (byte " << j << "): ";

        unsigned int lastHitCount = 0;
        unsigned int lastHitCountTimes = 0;
        for (std::list<unsigned int>::iterator shcIt = sortedHitCounts.begin();
            shcIt != sortedHitCounts.end(); ++shcIt)
        {
            if (*shcIt == lastHitCount)
            {
                /* This is the last hit count again for a
                 * different value.  Add it to the number of
                 * times I saw it.
                 */
                ++lastHitCountTimes;
            }
            else
            {
                /* A new, lesser (because sorted) hit count.
                 * Print the previous one.
                 */
                if (lastHitCountTimes > 0)
                {
                    std::cout << lastHitCount << " (" << lastHitCountTimes << " times), ";
                }

                /* Set the last hit count to the current
                 * value and reset the number of times I
                 * saw it.
                 */
                lastHitCount = *shcIt;
                lastHitCountTimes = 1;
            }
        }

        /* Print the final hit count */
        if (lastHitCountTimes > 0)
        {
            std::cout << lastHitCount << " (" << lastHitCountTimes << " times)" << std::endl;
        }
    }
}

void test_rngs(void)
{
    boost::random::random_device rdev;
    srand(rdev());

    /* If I'm going to be using OpenSSL, the default
     * engine needs initialising.
     */
    ENGINE_load_builtin_engines();
    ENGINE_register_all_complete();

#define TEST_CELLS_SIZE 1004
    AFK_Cell testCells[TEST_CELLS_SIZE];
    testCells[0] = afk_cell(afk_vec4<long long>(0, 0, 0, 1));
    for (unsigned int i = 1; i < TEST_CELLS_SIZE; ++i)
    {
        unsigned int randomScale = rand();
        if (randomScale & 0x40000000)
            randomScale = 1 << (randomScale & 0x1f);

        testCells[i] = afk_cell(afk_vec4<long long>(rand(), rand(), rand(), randomScale));
    }

    AFK_C_RNG                   c_rng;
    AFK_Boost_Taus88_RNG        boost_taus88_rng;
    AFK_Boost_MT19937_RNG       boost_mt19937_rng;
    AFK_AES_RNG                 aes_rng;
    AFK_HMAC_RNG                hmac_md5_rng(AFK_HMAC_Algo_MD5);
    AFK_HMAC_RNG                hmac_ripemd160_rng(AFK_HMAC_Algo_RIPEMD160);
    AFK_HMAC_RNG                hmac_sha1_rng(AFK_HMAC_Algo_SHA1);
    AFK_HMAC_RNG                hmac_sha256_rng(AFK_HMAC_Algo_SHA256);

#define RANDS_PER_CELL 100000
    evaluate_rng(c_rng, "C", testCells, TEST_CELLS_SIZE, RANDS_PER_CELL);
    evaluate_rng(boost_taus88_rng, "boost_taus88", testCells, TEST_CELLS_SIZE, RANDS_PER_CELL);
    evaluate_rng(boost_mt19937_rng, "boost_mt19937", testCells, TEST_CELLS_SIZE, RANDS_PER_CELL);
    evaluate_rng(aes_rng, "aes", testCells, TEST_CELLS_SIZE, RANDS_PER_CELL);
    evaluate_rng(hmac_md5_rng, "hmac_md5", testCells, TEST_CELLS_SIZE, RANDS_PER_CELL);
    evaluate_rng(hmac_ripemd160_rng, "hmac_ripemd160", testCells, TEST_CELLS_SIZE, RANDS_PER_CELL);
    evaluate_rng(hmac_sha1_rng, "hmac_sha1", testCells, TEST_CELLS_SIZE, RANDS_PER_CELL);
    evaluate_rng(hmac_sha256_rng, "hmac_sha256", testCells, TEST_CELLS_SIZE, RANDS_PER_CELL);
}

