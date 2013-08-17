/* AFK (c) Alex Holloway 2013 */
#ifndef _AFK_RNG_RNG_H_
#define _AFK_RNG_RNG_H_

#include <iostream>

/* We do lots of rotates ... */
#define LROTATE_UNSIGNED(v, r) (((v) << (r)) | ((v) >> (sizeof((v)) * 8 - (r))))

/* An RNG abstraction that lets me get the same interface
 * onto all the algorithms I try.
 */

class AFK_RNG_Value
{
public:
    union
    {
        unsigned char b[/* 16 */ 20]; /* allowing room for 160 bit hash functions, dropping top bits */
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

    /* Creates an AFK_RNG_Value out of four long longs,
     * making sure to distribute bits of all four into
     * all four words (necessary for the RNGs that are
     * composed out of 4 32-bit RNGs)
     */
    AFK_RNG_Value(long long v0, long long v1, long long v2, long long v3);

    /* Similar, but with just three long longs */
    AFK_RNG_Value(long long v0, long long v1, long long v2);

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

    /* Override this with the manner of re-seeding the underlying
     * RNG.
     */
    virtual void seed_internal(const AFK_RNG_Value& seed) = 0;

public:
    AFK_RNG() { vForFField = 4; }
    virtual ~AFK_RNG() {}

    void seed(const AFK_RNG_Value& seed);
    virtual AFK_RNG_Value rand(void) = 0;

    /* Returns a random unsigned int */
    unsigned int uirand(void)
    {
        if (vForFField == 4)
        {
            lastVForF = rand();
            vForFField = 0;
        }
    
        return lastVForF.v.ui[vForFField++];
    }

    /* Returns a random float between 0.0 and 1.0 */
    float frand(void)
    {
        return uirand() / 0x1.0p32;
    }
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

