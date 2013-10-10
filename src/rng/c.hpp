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

#ifndef _AFK_RNG_C_H_
#define _AFK_RNG_C_H_

#include <stdlib.h>

#include "rng.hpp"

/* Defines the AFK RNG in terms of the default C one.
 * (This is probably going to be quite bad, but who
 * knows?
 */

class AFK_C_RNG: public AFK_RNG
{
protected:
    AFK_RNG_Value originalSeed;
    int xorTracker;

    char state[16];
    struct random_data buf;

    int bumpXorTracker(void);
    virtual void seed_internal(const AFK_RNG_Value& seed);

public:
    virtual AFK_RNG_Value rand(void);
};

#endif /* _AFK_RNG_C_H_ */

