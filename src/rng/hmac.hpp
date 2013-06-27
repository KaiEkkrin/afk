/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_RNG_HMAC_H_
#define _AFK_RNG_HMAC_H_

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include "rng.hpp"

enum AFK_HMAC_RNG_Algorithm
{
    AFK_HMAC_Algo_MD5,
    AFK_HMAC_Algo_SHA1,
    AFK_HMAC_Algo_SHA256,
    AFK_HMAC_Algo_RIPEMD160
};
    

class AFK_HMAC_RNG: public AFK_RNG
{
protected:
    const EVP_MD *evp_md;
    unsigned int valLen;
    unsigned char *key;
    AFK_RNG_Value buffered;
    unsigned int counter;

    HMAC_CTX *ctx;

public:
    AFK_HMAC_RNG(enum AFK_HMAC_RNG_Algorithm algo);
    virtual ~AFK_HMAC_RNG();

    virtual void seed(const AFK_RNG_Value& seed);
    virtual AFK_RNG_Value rand(void);
};

#endif /* _AFK_RNG_HMAC_H_ */

