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
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <iostream>

#include "clock.hpp"

#ifdef AFK_GLX

afk_clock::time_point afk_clock::now()
{
    struct timespec ts;

    int result = clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    if (result != 0)
    {
        /* This is fatal (and shouldn't happen.) */
        std::cerr << "afk_clock: error getting time: " << strerror(errno) << std::endl;
        assert(false);
        exit(result);
    }

    return time_point(duration(ts.tv_nsec) +
        std::chrono::duration_cast<duration>(
            std::chrono::seconds(ts.tv_sec)));
}

#endif /* AFK_GLX */

#ifdef AFK_WGL

afk_clock::time_point afk_clock::now()
{
    /* Keep hold of the frequency. */
    static afk_thread_local bool haveFrequency = false;
    static afk_thread_local LARGE_INTEGER frequency;
    if (!haveFrequency)
    {
        if (!QueryPerformanceFrequency(&frequency))
        {
            /* This is fatal, and shouldn't happen. */
            std::cerr << "afk_clock: error querying performance frequency: " << GetLastError();
            assert(false);
            exit(1);
        }

        haveFrequency = true;
    }

    LARGE_INTEGER counter;
    if (!QueryPerformanceCounter(&counter))
    {
        /* This is also fatal, and shouldn't happen. */
        std::cerr << "afk_clock: error querying performance counter: " << GetLastError();
        assert(false);
        exit(1);
    }

    return time_point(duration(static_cast<rep>(
        (double)counter.QuadPart / (double)frequency.QuadPart * period::den / period::num)));
}

#endif /* AFK_WGL */

