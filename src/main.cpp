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

#include "afk.hpp"

#include <cassert>
#include <iostream>

#include "async/async_test.hpp"
#include "data/cache_test.hpp"
#include "data/chain_link_test.hpp"
#include "hash_test.hpp"
#include "rng/boost_taus88.hpp"
#include "rng/rng_test.hpp"
#include "test_jigsaw_fake3d.hpp"

#include "clock.hpp"
#include "core.hpp"
#include "exception.hpp"


#define TEST_ASYNC 0
#define TEST_CACHE 0
#define TEST_CHAIN_LINK 0
#define TEST_HASH 0
#define TEST_JIGSAW_FAKE3D 0
#define TEST_RNGS 0
#define TEST_SUBSTRATE 0


/* This is the AFK global core declared in core.hpp */
AFK_Core afk_core;


#ifdef _WIN32

#include "win32/arglist.hpp"
#include "win32/winconsole.hpp"

int APIENTRY WinMain(
    HINSTANCE   hInstance,
    HINSTANCE   hPrevInstance,
    LPSTR       lpCmdLine,
    int         nCmdShow)
{
    AFK_WinConsole winConsole;
    AFK_ArgList argList;
    int argc = argList.getArgc();
    char **argv = argList.getArgv();
#endif /* _WIN32 */

#ifdef __GNUC__
int main(int argc, char **argv)
{
#endif /* __GNUC__ */
    int retcode = 0;

#if TEST_ASYNC
    test_async();
    std::cin.ignore();
#endif

#if TEST_CACHE
    test_cache();
    std::cin.ignore();
#endif

#if TEST_CHAIN_LINK
    /* Edit iteration count here. */
    const int chainLinkTestIterations = 100;
    int chainLinkTestFails = 0;
    for (int i = 0; i < chainLinkTestIterations; ++i)
    {
        chainLinkTestFails += afk_testChainLink();
        std::cout << "Current total fails: " << chainLinkTestFails << std::endl;
    }

    std::cout << "Completed " << chainLinkTestIterations << " of the chain link test; " << chainLinkTestFails << " fails." << std::endl;
    std::cin.ignore();
#endif

#if TEST_HASH
    test_rotate();
    test_tileHash();
    test_cellHash();
    std::cin.ignore();
#endif

#if TEST_JIGSAW_FAKE3D
    test_jigsawFake3D();
    std::cin.ignore();
#endif

#if TEST_SUBSTRATE
    test_substrate();
    std::cin.ignore();
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
        std::cin.ignore();
#endif

        std::cout << "AFK initalising graphics" << std::endl;
        afk_core.initGraphics();

        std::cout << "AFK starting loop" << std::endl;
        afk_core.loop();
    }
    catch (AFK_Exception& e)
    {
        retcode = 1;
        std::cerr << "AFK Error: " << e.what() << std::endl;
        afk_clock::time_point now = afk_clock::now();
        afk_core.checkpoint(now, true);
    }

    std::cout << "AFK exiting" << std::endl;
    return retcode;
}

