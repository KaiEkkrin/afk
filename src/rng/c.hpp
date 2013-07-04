/* AFK (c) Alex Holloway 2013 */

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

