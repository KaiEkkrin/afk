/* AFK (c) Alex Holloway 2013 */
#ifndef _AFK_RNG_RNG_H_
#define _AFK_RNG_RNG_H_

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

    AFK_RNG_Value() {}
    AFK_RNG_Value(const AFK_RNG_Value& value)
    {
        v.ll[0] = value.v.ll[0];
        v.ll[1] = value.v.ll[1];
    }

    AFK_RNG_Value& operator=(const AFK_RNG_Value& value)
    {
        v.ll[0] = value.v.ll[0];
        v.ll[1] = value.v.ll[1];
        return *this;
    }
};

class AFK_RNG
{
protected:
    /* Tracks the default way of making floats */
    AFK_RNG_Value lastVForF;
    int vForFField;

public:
    AFK_RNG() { vForFField = 4; }

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

