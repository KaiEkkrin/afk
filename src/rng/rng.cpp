/* AFK (c) Alex Holloway 2013 */

#include <stdlib.h>

#include "rng.hpp"


/* RNG_Value stuff. */

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

float AFK_RNG::frand(void)
{
    if (vForFField == 4)
    {
        lastVForF = rand();
        vForFField = 0;
    }

    unsigned int basis = lastVForF.v.ui[vForFField++];
    return (float)basis / 0x1.0p32;
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

