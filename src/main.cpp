/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <iostream>

#include "async/test.hpp"
#include "data/cache_test.hpp"
#include "hash_test.hpp"
#include "rng/boost_taus88.hpp"
#include "rng/test.hpp"
#include "test_jigsaw_fake3d.hpp"

#include "core.hpp"
#include "exception.hpp"


#define TEST_ASYNC 0
#define TEST_CACHE 0
#define TEST_HASH 0
#define TEST_JIGSAW_FAKE3D 0
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

#if TEST_HASH
    test_rotate();
    test_tileHash();
    test_cellHash();
#endif

#if TEST_JIGSAW_FAKE3D
    test_jigsawFake3D();
#endif

#if TEST_SUBSTRATE
    test_substrate();
#endif

    try
    {
        std::cout << "AFK configuring" << std::endl;
        afk_core.configure(&argc, argv);

        std::cout << "AFK Using master seed: " << afk_core.config->masterSeed << std::endl;

		/* The RNG test needs to go here after that master seed has
		 * been initialised, because it'll refer to it.
		 */
#if TEST_RNGS
    	test_rngs();
#endif

        std::cout << "AFK initalising graphics" << std::endl;
        afk_core.initGraphics();

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

