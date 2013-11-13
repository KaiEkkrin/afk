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

#include <errno.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
        exit(result);
    }

    return time_point(duration(ts.tv_nsec) +
        std::chrono::duration_cast<duration>(
            std::chrono::seconds(ts.tv_sec)));
}

#endif /* AFK_GLX */

#ifdef AFK_WGL

/* TODO. See clock.hpp */

#endif /* AFK_WGL */

