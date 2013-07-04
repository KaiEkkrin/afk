/* AFK (c) Alex Holloway 2013 */

#include <stdlib.h>

#include "rng.hpp"


/* RNG_Value stuff. */

static unsigned int squash_to_int(long long in)
{
    unsigned int out, mid;

    out = (unsigned int)(in & 0x00000000ffffffff);
    mid = (unsigned int)((in & 0xffffffff00000000) >> 32);

    asm("rol $19, %0\n" :"=r"(mid) :"0"(mid));

    return out ^ mid;
}

AFK_RNG_Value::AFK_RNG_Value(long long v0, long long v1, long long v2, long long v3)
{
    asm("rol $17, %0\n": "=r"(v1) :"0"(v1));
    asm("rol $31, %0\n": "=r"(v2) :"0"(v2));
    asm("rol $47, %0\n": "=r"(v3) :"0"(v3));

    v.ui[0] = v.ui[1] = v.ui[2] = v.ui[3] =
        squash_to_int(v0) ^
        squash_to_int(v1) ^
        squash_to_int(v2) ^
        squash_to_int(v3);

    asm("rol $7, %0\n": "=r"(v.ui[0]) :"0"(v.ui[0]));
    asm("rol $13, %0\n": "=r"(v.ui[1]) :"0"(v.ui[1]));
    asm("rol $23, %0\n": "=r"(v.ui[2]) :"0"(v.ui[2]));
    asm("rol $29, %0\n": "=r"(v.ui[3]) :"0"(v.ui[3]));
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

unsigned int AFK_RNG::uirand(void)
{
    if (vForFField == 4)
    {
        lastVForF = rand();
        vForFField = 0;
    }
    
    return lastVForF.v.ui[vForFField++];
}

float AFK_RNG::frand(void)
{
    return uirand() / 0x1.0p32;
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

