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

#include <cstring>
#include <functional>
#include <iostream>
#include <list>

#include <boost/random/random_device.hpp>

#include "boost_mt19937.hpp"
#include "boost_taus88.hpp"
#include "rng_test.hpp"
#include "../afk.hpp"
#include "../cell.hpp"
#include "../clock.hpp"
#include "../file/logstream.hpp"


/* TODO Add a test that tweaks each coord in an AFK_Cell in
 * turn, and checks that the RNG output changes entirely.
 * (Important for RNGs composed out of several smaller ones,
 * like the taus88 implementation.)
 */


static void evaluate_rng(AFK_RNG& rng, const std::string& name, const AFK_Cell* testCells, const size_t testCellCount, const size_t randsPerCell)
{
    AFK_RNG_Test rng_test(&rng, 20, 1000);

    afk_out << "Testing RNG: " << name << std::endl;

    /* The first 10 test cells are used for the contribution test. */   
    unsigned int i;
    for (i = 0; i < 10; ++i)
        rng_test.contribute(testCells[i].rngSeed());

    rng_test.printResults();

    /* now do a performance test.  How long does it take
     * to fill many many floats ?
     */

    afk_out << "Performance test: trying " << (testCellCount / 2) - i << " cells taking " << randsPerCell << " randoms each" << std::endl;

    float smallestValue = 1.0f, largestValue = 0.0f;
    float *manyFloats = new float[randsPerCell];
    float *sampleFloats = new float[testCellCount];

    afk_clock::time_point ptestStartTime = afk_clock::now();
    for (; i < testCellCount / 2; ++i)
    {
        rng.seed(testCells[i].rngSeed());
        for (unsigned int j = 0; j < randsPerCell; ++j)
            manyFloats[j] = rng.frand();

        sampleFloats[i] = manyFloats[rng.rand().v.ui[0] % randsPerCell];
    }
    float sampleFloat = sampleFloats[rng.rand().v.ui[0] % testCellCount / 2];
    afk_clock::time_point ptestEndTime = afk_clock::now();
    afk_duration_mfl timeTaken = std::chrono::duration_cast<afk_duration_mfl>(
        ptestEndTime - ptestStartTime);

    afk_out << "Test took " << timeTaken.count() << " millis with " << name << std::endl;
    afk_out << "Smallest value: " << smallestValue << std::endl;
    afk_out << "Largest value: " << largestValue << std::endl;
    afk_out << "Random float from the set: " << sampleFloat << std::endl;

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

    ptestStartTime = afk_clock::now();
    for (; i < testCellCount; ++i)
    {
        rng.seed(testCells[i].rngSeed());
        AFK_RNG_Value val = rng.rand();

        for (int j = 0; j < 16; j += 4)
            for (int k = 0; k < 4; ++k)
                ++hits[k][val.v.b[j+k]];
    }
    ptestEndTime = afk_clock::now();
    timeTaken = std::chrono::duration_cast<afk_duration_mfl>(
        ptestEndTime - ptestStartTime);
    afk_out << "Hit count test took " << timeTaken.count() << " millis with " << name << std::endl;

    /* Do the analysis. */
    for (int j = 0; j < 4; ++j)
    {
        std::list<unsigned int> sortedHitCounts;

        for (int k = 0; k < 256; ++k)
            sortedHitCounts.push_back(hits[j][k]);

        sortedHitCounts.sort(std::greater<unsigned int>());

        afk_out << "Hit counts (byte " << j << "): ";

        unsigned int lastHitCount = 0;
        unsigned int lastHitCountTimes = 0;
        for (auto shc : sortedHitCounts)
        {
            if (shc == lastHitCount)
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
                    afk_out << lastHitCount << " (" << lastHitCountTimes << " times), ";
                }

                /* Set the last hit count to the current
                 * value and reset the number of times I
                 * saw it.
                 */
                lastHitCount = shc;
                lastHitCountTimes = 1;
            }
        }

        /* Print the final hit count */
        if (lastHitCountTimes > 0)
        {
            afk_out << lastHitCount << " (" << lastHitCountTimes << " times)" << std::endl;
        }
    }
}

void test_rngs(void)
{
    boost::random::random_device rdev;
    srand(rdev());

#define TEST_CELLS_SIZE 1004
    AFK_Cell testCells[TEST_CELLS_SIZE];
    testCells[0] = afk_cell(afk_vec4<int64_t>(0, 0, 0, 1));
    for (unsigned int i = 1; i < TEST_CELLS_SIZE; ++i)
    {
        unsigned int randomScale = rand();
        if (randomScale & 0x40000000)
            randomScale = 1 << (randomScale & 0x1f);

        testCells[i] = afk_cell(afk_vec4<int64_t>(rand(), rand(), rand(), randomScale));
    }

    AFK_Boost_Taus88_RNG        boost_taus88_rng;
    AFK_Boost_MT19937_RNG       boost_mt19937_rng;

#define RANDS_PER_CELL 100000
    evaluate_rng(boost_taus88_rng, "boost_taus88", testCells, TEST_CELLS_SIZE, RANDS_PER_CELL);
    evaluate_rng(boost_mt19937_rng, "boost_mt19937", testCells, TEST_CELLS_SIZE, RANDS_PER_CELL);
}

