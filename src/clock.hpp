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

#ifndef _AFK_CLOCK_H_
#define _AFK_CLOCK_H_

#include <chrono>

#ifdef AFK_GLX

/* The default clock implementation is all naffed up, so here's a Linux
 * specific thing, matching the std::chrono interface.
 */
class afk_clock
{
public:
    typedef std::chrono::nanoseconds                        duration;
    typedef duration::rep                                   rep;
    typedef duration::period                                period;
    typedef std::chrono::time_point<afk_clock, duration>    time_point;

    static constexpr bool is_steady = true;
    static time_point now();
};

typedef std::chrono::duration<float, std::ratio<1, 1000> > afk_duration_mfl;

#endif /* AFK_GLX */

#ifdef AFK_WGL
/* TODO: On Windows, the Visual Studio implementation of high_resolution_timer
 * is broken, and this will be slow and imprecise.
 * See http://stackoverflow.com/questions/13263277/difference-between-stdsystem-clock-and-stdsteady-clock
 */
#endif /* AFK_WGL */

#endif /* _AFK_CLOCK_H_ */

