/* AFK (c) Alex Holloway 2013 */
#ifndef _AFK_RNG_RNG_H_
#define _AFK_RNG_RNG_H_

#include <iostream>

/* An RNG abstraction that lets me get the same interface
 * onto all the algorithms I try.
 */

class AFK_RNG_Value
{
public:
    union
    {
        unsigned char b[/* 16 */ 20]; /* TODO allowing room for 160 bit hash functions, dropping top bits */
        int i[4];
        unsigned int ui[4];
        long long ll[2];
        unsigned long long ull[2];
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

public:
    AFK_RNG() { vForFField = 4; }
    virtual ~AFK_RNG() {}

    virtual void seed(const AFK_RNG_Value& seed) = 0;
    virtual AFK_RNG_Value rand(void) = 0;

    /* Returns a random float between 0 and 1 */
    virtual float frand(void);
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

