/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_RNG_BOOST_TAUS88_H_
#define _AFK_RNG_BOOST_TAUS88_H_

#include <boost/random/taus88.hpp>

#include "rng.hpp"

class AFK_Boost_Taus88_RNG: public AFK_RNG
{
protected:
    boost::random::taus88 rng[4];

public:
    virtual void seed(const AFK_RNG_Value& seed);
    virtual AFK_RNG_Value rand(void);
};

#endif /* _AFK_RNG_BOOST_TAUS88_H_ */

