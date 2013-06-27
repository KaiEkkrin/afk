/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_RNG_AES_H_
#define _AFK_RNG_AES_H_

#include <openssl/evp.h>

#include "rng.hpp"

class AFK_AES_RNG: public AFK_RNG
{
protected:
    const EVP_CIPHER *evp_cipher;
    AFK_RNG_Value key;
    unsigned int counter[4];

    EVP_CIPHER_CTX *ctx;

public:
    AFK_AES_RNG();
    virtual ~AFK_AES_RNG();

    virtual void seed(const AFK_RNG_Value& seed);
    virtual AFK_RNG_Value rand(void);
};

#endif /* _AFK_RNG_AES_H_ */

