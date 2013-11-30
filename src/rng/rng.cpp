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

#include <cstdlib>

#include "rng.hpp"


/* RNG_Value stuff. */

static unsigned int squash_to_int(uint64_t in)
{
    unsigned int out, mid;

    out = (unsigned int)(in & 0x00000000ffffffff);
    mid = (unsigned int)((in & 0xffffffff00000000) >> 32);

    return out ^ AFK_LROTATE_UNSIGNED(mid, 19);
}

/* TODO: I really should stop having a native 128-bit seed, now that I've
 * decided not to go for any crypto RNGs.
 * Humph
 */

AFK_RNG_Value::AFK_RNG_Value(int64_t v0, int64_t v1, int64_t v2, int64_t v3)
{
    v.ui[0] = v.ui[1] = v.ui[2] = v.ui[3] =
        squash_to_int(v0) ^
        squash_to_int(AFK_LROTATE_UNSIGNED((uint64_t)v1, 17)) ^
        squash_to_int(AFK_LROTATE_UNSIGNED((uint64_t)v2, 31)) ^
        squash_to_int(AFK_LROTATE_UNSIGNED((uint64_t)v3, 47));

    v.ui[0] = AFK_LROTATE_UNSIGNED(v.ui[0], 7);
    v.ui[1] = AFK_LROTATE_UNSIGNED(v.ui[1], 13);
    v.ui[2] = AFK_LROTATE_UNSIGNED(v.ui[2], 23);
    v.ui[3] = AFK_LROTATE_UNSIGNED(v.ui[3], 29);
}

AFK_RNG_Value::AFK_RNG_Value(int64_t v0, int64_t v1, int64_t v2)
{
    v.ui[0] = v.ui[1] = v.ui[2] = v.ui[3] =
        squash_to_int(v0) ^
        squash_to_int(AFK_LROTATE_UNSIGNED((uint64_t)v1, 19)) ^
        squash_to_int(AFK_LROTATE_UNSIGNED((uint64_t)v2, 41));

    v.ui[0] = AFK_LROTATE_UNSIGNED(v.ui[0], 7);
    v.ui[1] = AFK_LROTATE_UNSIGNED(v.ui[1], 13);
    v.ui[2] = AFK_LROTATE_UNSIGNED(v.ui[2], 23);
}

AFK_RNG_Value::AFK_RNG_Value(int64_t v0)
{
    v.ui[0] = squash_to_int(v0);
    v.ui[1] = AFK_LROTATE_UNSIGNED(squash_to_int(v0), 7);
    v.ui[2] = AFK_LROTATE_UNSIGNED(squash_to_int(v0), 13);
    v.ui[3] = AFK_LROTATE_UNSIGNED(squash_to_int(v0), 23);
}

AFK_RNG_Value::AFK_RNG_Value(const std::string& s1, const std::string& s2)
{
    v.ull[0] = strtoull(s1.c_str(), NULL, 0);
    v.ull[1] = strtoull(s2.c_str(), NULL, 0);
}

std::ostream& operator<<(std::ostream& os, const AFK_RNG_Value& value)
{
    return os << "0x" << std::hex << value.v.ull[0] << " 0x" << std::hex << value.v.ull[1];
}


/* Actual RNG utilities. */

void AFK_RNG::seed(const AFK_RNG_Value& seed)
{
    /* Call the internal function to do the actual seeding. */
    seed_internal(seed);

    /* Reset the float tracker.  (very important, otherwise
     * I get one of 4 possible states!)
     */
    vForFField = 4;
}


/* The RNG test system. */

AFK_RNG_Test::AFK_RNG_Test(AFK_RNG *_rng, unsigned int _divisions, unsigned int _iterations)
{
    rng = _rng;
    stats = new unsigned int[_divisions+1]; /* account for inaccuracy resulting in >1.0f floats */
    divisions = _divisions;
    iterations = _iterations;
    stride = 1.0f / (float)divisions;

    for (unsigned int i = 0; i < divisions+1; ++i)
        stats[i] = 0;
}

AFK_RNG_Test::~AFK_RNG_Test()
{
    delete[] stats;
}

void AFK_RNG_Test::contribute(const AFK_RNG_Value& seed)
{
    rng->seed(seed);

    for (unsigned int i = 0; i < iterations; ++i)
    {
        unsigned int d;
        float v = rng->frand();

        for (d = 0; d < divisions; ++d)
        {
            if (v < (stride * (float)(d+1)))
            {
                ++(stats[d]);
                break;
            }
        }

        if (d == divisions) /* RNG spat out a >1.0f value */
            ++(stats[divisions]);
    }
}

void AFK_RNG_Test::printResults(void) const
{
    for (unsigned int d = 0; d < divisions+1; ++d)
        std::cout << (stride * (float)d) << " - " << (stride * (float)(d + 1)) << ": " << stats[d] << std::endl;
}

