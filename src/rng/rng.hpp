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
#ifndef _AFK_RNG_RNG_H_
#define _AFK_RNG_RNG_H_

#include <cstdint>
#include <iostream>

#include "hash.hpp"

/* We do lots of rotates ... */

/* An RNG abstraction that lets me get the same interface
 * onto all the algorithms I try.
 */

class AFK_RNG_Value
{
public:
    union
    {
        uint8_t b[/* 16 */ 20]; /* allowing room for 160 bit hash functions, dropping top bits */
        int32_t i[4];
        uint32_t ui[4];
        int64_t ll[2];
        uint64_t ull[2];
    } v;

    AFK_RNG_Value()
    {
        v.ull[0] = v.ull[1] = 0uLL;
    }

    AFK_RNG_Value(const AFK_RNG_Value& value)
    {
        v.ull[0] = value.v.ull[0];
        v.ull[1] = value.v.ull[1];
    }

    /* Creates an AFK_RNG_Value out of four int64_ts,
     * making sure to distribute bits of all four into
     * all four words (necessary for the RNGs that are
     * composed out of 4 32-bit RNGs)
     */
    AFK_RNG_Value(int64_t v0, int64_t v1, int64_t v2, int64_t v3);

    /* Similar, but with just three int64_ts */
    AFK_RNG_Value(int64_t v0, int64_t v1, int64_t v2);

    /* And again, but with just one */
    AFK_RNG_Value(int64_t v0);

    /* Parsing constructor. */
    AFK_RNG_Value(const std::string& s1, const std::string& s2);

    AFK_RNG_Value& operator=(const AFK_RNG_Value& value)
    {
        v.ull[0] = value.v.ull[0];
        v.ull[1] = value.v.ull[1];
        return *this;
    }

    /* For combining a modifier with a base seed. */
    AFK_RNG_Value operator^(const AFK_RNG_Value& value)
    {
        AFK_RNG_Value newValue(value);
        newValue.v.ull[0] ^= v.ull[0];
        newValue.v.ull[1] ^= v.ull[1];
        return newValue;
    }

    AFK_RNG_Value& operator^=(const AFK_RNG_Value& value)
    {
        v.ull[0] ^= value.v.ull[0];
        v.ull[1] ^= value.v.ull[1];
        return *this;
    }
};

/* For printing an AFK_RNG_Value. */
std::ostream& operator<<(std::ostream& os, const AFK_RNG_Value& value);

class AFK_RNG
{
protected:
    /* Tracks the default way of making floats */
    AFK_RNG_Value lastVForF;
    int vForFField;

    /* Override this with the manner of re-seeding the underlying
     * RNG.
     */
    virtual void seed_internal(const AFK_RNG_Value& seed) = 0;

public:
    AFK_RNG() { vForFField = 4; }
    virtual ~AFK_RNG() {}

    void seed(const AFK_RNG_Value& seed);
    virtual AFK_RNG_Value rand(void) = 0;

    /* Returns a random unsigned int */
    uint32_t uirand(void)
    {
        if (vForFField == 4)
        {
            lastVForF = rand();
            vForFField = 0;
        }
    
        return lastVForF.v.ui[vForFField++];
    }

    /* Returns a random float between 0.0 and 1.0 */
    float frand(void)
    {
        return (float)uirand() / 4294967296.0f;
    }
};

class AFK_RNG_Test
{
protected:
    AFK_RNG *rng;
    unsigned int *stats;
    unsigned int divisions;
    unsigned int iterations;
    float stride;

public:
    AFK_RNG_Test(AFK_RNG *_rng, unsigned int _divisions, unsigned int _iterations);
    ~AFK_RNG_Test();

    void contribute(const AFK_RNG_Value& seed);
    void printResults(void) const;
};

#endif /* _AFK_RNG_RNG_H_ */

