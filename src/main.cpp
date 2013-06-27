/* AFK (c) Alex Holloway 2013 */

/* TODO Convert this stuff to C++11.  It's genuinely better and I'm already
 * noticing places where I would benefit from using it instead:
 * - use unique_ptr instead of auto_ptr
 * - use unordered_map instead of map
 * - use { ... } initialisation assignments instead of cumbersome messes
 */

#include "afk.hpp"

#include <iostream>

#include <boost/date_time/posix_time/posix_time.hpp>

#include "core.hpp"
#include "exception.hpp"
#include "landscape.hpp"
#include "rng/c.hpp"
#include "rng/rng.hpp"

/* This is the AFK global core declared in core.h */
AFK_Core afk_core;


int main(int argc, char **argv)
{
    int retcode = 0;

    /* TODO REMOVEME Test the RNG; take this away after
     * I've chosen something sensible
     */
    {
        AFK_C_RNG c_rng;
        AFK_RNG_Test c_rng_test(&c_rng, 20, 1000);

        c_rng_test.contribute(long_hash_value(AFK_Cell(Vec4<int>(0, 0, 0, 1))));
        c_rng_test.contribute(long_hash_value(AFK_Cell(Vec4<int>(1, 0, 1, 1))));
        c_rng_test.contribute(long_hash_value(AFK_Cell(Vec4<int>(16384, 16384, 0, 16384))));
        c_rng_test.contribute(long_hash_value(AFK_Cell(Vec4<int>(103, -123, 6183, 2))));

        c_rng_test.printResults();

        /* now do a performance test.  How long does it take
         * to fill many many floats ?
         */
        const unsigned int pTestCount = 100000000;
        float *manyFloats = new float[pTestCount];
        float smallestValue = 1.0f, largestValue = 0.0f;
        c_rng.seed(long_hash_value(AFK_Cell(Vec4<int>(478, 1188, -780, 2))));
        boost::posix_time::ptime ptestStartTime = boost::posix_time::microsec_clock::local_time();
        
        for (unsigned int i = 0; i < pTestCount; ++i)
        {
            manyFloats[i] = c_rng.frand();
            if (manyFloats[i] < smallestValue) smallestValue = manyFloats[i];
            if (manyFloats[i] > largestValue) largestValue = manyFloats[i];
        }

        boost::posix_time::ptime ptestEndTime = boost::posix_time::microsec_clock::local_time();

        std::cout << "Constructing " << pTestCount << " floats took " << (ptestEndTime - ptestStartTime) << " with AFK_C_RNG" << std::endl;
        std::cout << "Smallest value: " << smallestValue << std::endl;
        std::cout << "Largest value: " << largestValue << std::endl;
        std::cout << "Random float from the set: " << manyFloats[c_rng.rand().v.ui[0] % pTestCount] << std::endl;

        delete[] manyFloats;
    }

    try
    {
        std::cout << "AFK initalising graphics" << std::endl;
        afk_core.initGraphics(&argc, argv);

        //std::cout << "AFK initialising compute" << std::endl;
        afk_core.initCompute();

        std::cout << "AFK configuring" << std::endl;
        afk_core.configure(&argc, argv);

        std::cout << "AFK starting loop" << std::endl;
        afk_core.loop();
    }
    catch (AFK_Exception e)
    {
        retcode = 1;
        std::cerr << "AFK Error: " << e.what() << std::endl;
    }

    std::cout << "AFK exiting" << std::endl;
    afk_core.printOccasionals(true);
    return retcode;
}

