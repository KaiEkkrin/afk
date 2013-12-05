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
#include "hash_test.hpp"
#include "rng/boost_taus88.hpp"
#include "rng/rng_test.hpp"
#include "test_jigsaw_fake3d.hpp"

#include "clock.hpp"
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


#ifdef _WIN32
#include <shellapi.h>

class AFK_ArgList
{
protected:
    LPWSTR *argvw;

    int argc;
    char **argv;

public:
    AFK_ArgList() : argvw(nullptr), argc(0), argv(nullptr)
    {
        /* Using the shellapi, I can extract a Unix style argv, albeit only in wide char format: */
        LPWSTR fullCmdLineW = GetCommandLineW();
        assert(fullCmdLineW);

        argvw = CommandLineToArgvW(fullCmdLineW, &argc);
        if (!argvw)
        {
            std::ostringstream ss;
            ss << "CommandLineToArgvW failed: " << GetLastError();
            throw AFK_Exception(ss.str());
        }

        /* Convert those wide strings into something sane.
         * TODO: Yes, I know, this means AFK isn't supporting unicode paths.
         * I need to come up with something sensible, like converting to UTF-8
         * here.
         */
        argv = new char *[argc];
        for (int i = 0; i < argc; ++i)
        {
            /* Work out how much space I need for this argument */
            int requiredSize = WideCharToMultiByte(
                CP_ACP,
                0,
                argvw[i],
                -1, /* CommandLineToArgvW must produce null terminated strings */
                nullptr,
                0,
                0,
                0
                );

            if (requiredSize > 0)
            {
                argv[i] = new char[requiredSize];
                requiredSize = WideCharToMultiByte(
                    CP_ACP,
                    0,
                    argvw[i],
                    -1,
                    argv[i],
                    requiredSize,
                    0,
                    0
                    );
            }

            assert(requiredSize > 0);
            if (requiredSize == 0)
            {
                argv[i] = new char[1];
                argv[i][0] = '\0';
            }
        }
    }

    virtual ~AFK_ArgList()
    {
        if (argv)
        {
            for (int i = 0; i < argc; ++i) delete[] argv[i];
            delete[] argv;
        }

        if (argvw) LocalFree(argvw);
    }

    int getArgc(void) const { return argc; }
    char **getArgv(void) const { return argv; }
};

int APIENTRY WinMain(
    HINSTANCE   hInstance,
    HINSTANCE   hPrevInstance,
    LPSTR       lpCmdLine,
    int         nCmdShow)
{
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

