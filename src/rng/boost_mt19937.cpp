/* AFK (c) Alex Holloway 2013 */

#include "boost_mt19937.hpp"

void AFK_Boost_MT19937_RNG::seed_internal(const AFK_RNG_Value& seed)
{
    rng[0] = boost::random::mt19937(seed.v.i[0]);
    rng[1] = boost::random::mt19937(seed.v.i[1]);
    rng[2] = boost::random::mt19937(seed.v.i[2]);
    rng[3] = boost::random::mt19937(seed.v.i[3]);
}

AFK_RNG_Value AFK_Boost_MT19937_RNG::rand(void)
{
    AFK_RNG_Value v;

    v.v.i[0] = rng[0]();
    v.v.i[1] = rng[1]();
    v.v.i[2] = rng[2]();
    v.v.i[3] = rng[3]();

    return v;
}

