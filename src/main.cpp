/* AFK (c) Alex Holloway 2013 */

/* TODO Convert this stuff to C++11.  It's genuinely better and I'm already
 * noticing places where I would benefit from using it instead:
 * - use unique_ptr instead of auto_ptr
 * - use unordered_map instead of map
 * - use { ... } initialisation assignments instead of cumbersome messes
 */

#include "afk.hpp"

#include <iostream>

#include "async/test.hpp"
#include "data/cache_test.hpp"
#include "rng/boost_taus88.hpp"
#include "rng/test.hpp"

#include "core.hpp"
#include "exception.hpp"


#define TEST_ASYNC 0
#define TEST_CACHE 0
#define TEST_RNGS 0
#define TEST_SUBSTRATE 0


/* This is the AFK global core declared in core.h */
AFK_Core afk_core;


int main(int argc, char **argv)
{
    int retcode = 0;

#if TEST_ASYNC
    test_async();
#endif

#if TEST_CACHE
    test_cache();
#endif

#if TEST_RNGS
    test_rngs();
#endif

#if TEST_SUBSTRATE
    test_substrate();
#endif

    try
    {
        std::cout << "AFK initalising graphics" << std::endl;
        afk_core.initGraphics(&argc, argv);

        //std::cout << "AFK initialising compute" << std::endl;
        afk_core.initCompute();

        std::cout << "AFK configuring" << std::endl;
        afk_core.configure(&argc, argv);

        std::cout << "AFK Using master seed: " << afk_core.config->masterSeed << std::endl;

        std::cout << "AFK starting loop" << std::endl;
        afk_core.loop();
    }
    catch (AFK_Exception e)
    {
        retcode = 1;
        std::cerr << "AFK Error: " << e.what() << std::endl;
        boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
        afk_core.checkpoint(now, true);
    }

    std::cout << "AFK exiting" << std::endl;
    return retcode;
}

