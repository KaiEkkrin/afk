/* AFK (c) Alex Holloway 2013 */

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

    /* TODO This is only ever making positive integers,
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

