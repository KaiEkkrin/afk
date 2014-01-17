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

#ifndef _AFK_RNG_BOOST_TAUS88_H_
#define _AFK_RNG_BOOST_TAUS88_H_

#include <boost/random/taus88.hpp>

#include "rng.hpp"

class AFK_Boost_Taus88_RNG: public AFK_RNG
{
protected:
    boost::random::taus88 rng[4];

    virtual void seed_internal(const AFK_RNG_Value& seed);

public:
    virtual AFK_RNG_Value rand(void);
};

#endif /* _AFK_RNG_BOOST_TAUS88_H_ */

