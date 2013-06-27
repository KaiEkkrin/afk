/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_RNG_BOOST_MT19937_H_
#define _AFK_RNG_BOOST_MT19937_H_

#include <boost/random/mersenne_twister.hpp>

#include "rng.hpp"

class AFK_Boost_MT19937_RNG: public AFK_RNG
{
protected:
    boost::random::mt19937 rng[4];

public:
    virtual void seed(const AFK_RNG_Value& seed);
    virtual AFK_RNG_Value rand(void);
};

#endif /* _AFK_RNG_BOOST_MT19937_H_ */

