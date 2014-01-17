/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
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

#include "boost_taus88.hpp"

void AFK_Boost_Taus88_RNG::seed_internal(const AFK_RNG_Value& seed)
{
    rng[0] = boost::random::taus88(seed.v.i[0]);
    rng[1] = boost::random::taus88(seed.v.i[1]);
    rng[2] = boost::random::taus88(seed.v.i[2]);
    rng[3] = boost::random::taus88(seed.v.i[3]);
}

AFK_RNG_Value AFK_Boost_Taus88_RNG::rand(void)
{
    AFK_RNG_Value v;

    v.v.i[0] = rng[0]();
    v.v.i[1] = rng[1]();
    v.v.i[2] = rng[2]();
    v.v.i[3] = rng[3]();

    return v;
}

