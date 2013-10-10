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

#include "c.hpp"

#include <string.h>

int AFK_C_RNG::bumpXorTracker(void)
{
    int t = xorTracker;
    ++xorTracker;
    if (xorTracker == 4) xorTracker = 1;
    return t;
}

void AFK_C_RNG::seed_internal(const AFK_RNG_Value& seed)
{
    originalSeed = seed;
    xorTracker = 1;

    memset(state, 0, sizeof(state));
    memset(&buf, 0, sizeof(buf));

    initstate_r(seed.v.ui[0], state, sizeof(state), &buf); 
}

AFK_RNG_Value AFK_C_RNG::rand(void)
{
    AFK_RNG_Value v;

    /* This is only ever making positive integers,
     * i.e. I only get 31 random bits not 32.  ARGH
     */
    random_r(&buf, &v.v.i[0]);
    //v.v.ui[0] ^= originalSeed.v.ui[bumpXorTracker()];
    random_r(&buf, &v.v.i[1]);
    //v.v.ui[1] ^= originalSeed.v.ui[bumpXorTracker()];
    random_r(&buf, &v.v.i[2]);
    //v.v.ui[2] ^= originalSeed.v.ui[bumpXorTracker()];
    random_r(&buf, &v.v.i[3]);
    //v.v.ui[3] ^= originalSeed.v.ui[bumpXorTracker()];

    return v;
}

