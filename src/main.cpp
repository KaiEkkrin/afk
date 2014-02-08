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

#include "afk.hpp"

#include <cassert>
#include <iostream>

#include "async/async_test.hpp"
#include "data/cache_test.hpp"
#include "data/chain_link_test.hpp"
#include "file/logstream.hpp"
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

    winConsole.open();
#endif /* _WIN32 */

#ifdef __GNUC__
int main(int argc, char **argv)
{
#endif /* __GNUC__ */
    int retcode = 0;

#if TEST_ASYNC
    test_async();
    afk_waitForKeyPress();
#endif

#if TEST_CACHE
    test_cache();
    afk_waitForKeyPress();
#endif

#if TEST_CHAIN_LINK
    /* Edit iteration count here. */
    const int chainLinkTestIterations = 100;
    int chainLinkTestFails = 0;
    for (int i = 0; i < chainLinkTestIterations; ++i)
    {
        chainLinkTestFails += afk_testChainLink();
        afk_out << "Current total fails: " << chainLinkTestFails << std::endl;
    }

    afk_out << "Completed " << chainLinkTestIterations << " of the chain link test; " << chainLinkTestFails << " fails." << std::endl;
    afk_waitForKeyPress();
#endif

#if TEST_HASH
    test_rotate();
    test_tileHash();
    test_cellHash();
    afk_waitForKeyPress();
#endif

#if TEST_JIGSAW_FAKE3D
    test_jigsawFake3D();
    afk_waitForKeyPress();
#endif

#if TEST_SUBSTRATE
    test_substrate();
    afk_waitForKeyPress();
#endif

    bool hitLoop = false;
    try
    {
        afk_core.configure(&argc, argv);

        /* First things first, set up the logging: */
        std::string logFile = afk_core.settings.logFile;
        if (logFile.size() > 0) afk_logBacking.setLogFile(logFile);

        /* Banner. */
        afk_out << "AFK v0.2.1-test" << std::endl;
        afk_out << "Copyright (C) 2013-2014, Alex Holloway" << std::endl;
        afk_out << "This program is free software.  You should have found a copy of its" << std::endl;
        afk_out << "source code near this binary, or if you obtained it from GitHub, you" << std::endl;
        afk_out << "can check out the sources at https://github.com/KaiEkkrin/afk/ ." << std::endl;
        afk_out << std::endl;

        /* If we're not set to log to console, that's all we'll print to it */
        afk_logBacking.setConsole(afk_core.settings.console);

        /* Printing the configuration is useful. */
        afk_out << "AFK: Using starting configuration: " << std::endl << std::endl;
        afk_core.settings.saveTo(afk_out);
        afk_out << std::endl;

		/* The RNG test needs to go here after that master seed has
		 * been initialised, because it'll refer to it.
		 */
#if TEST_RNGS
    	test_rngs();
        afk_waitForKeyPress();
#endif

        afk_out << "AFK initalising graphics" << std::endl;
        afk_core.initGraphics();

        hitLoop = true;
        afk_out << "AFK starting loop" << std::endl;
        afk_core.loop();
    }
    catch (AFK_Exception& e)
    {
        retcode = 1;
        afk_out << "AFK Error: " << e.what() << std::endl;

        if (hitLoop)
        {
            /* Printing a checkpoint right at the end could be useful for
             * diagnosis.
             */
            afk_clock::time_point now = afk_clock::now();
            afk_core.checkpoint(now, true);
        }
    }

    afk_out << "AFK exiting" << std::endl;
    return retcode;
}
