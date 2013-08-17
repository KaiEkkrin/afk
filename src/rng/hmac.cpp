/* AFK (c) Alex Holloway 2013 */

#include <stdlib.h>
#include <string.h>

#include "hmac.hpp"

AFK_HMAC_RNG::AFK_HMAC_RNG(enum AFK_HMAC_RNG_Algorithm algo)
{
    switch (algo)
    {
    case AFK_HMAC_Algo_MD5:
        evp_md = EVP_md5();
        valLen = 16;
        break;

    case AFK_HMAC_Algo_SHA1:
        evp_md = EVP_sha1();
        valLen = 20;
        break;

    case AFK_HMAC_Algo_SHA256:
        evp_md = EVP_sha256();
        valLen = 32;
        break;

    case AFK_HMAC_Algo_RIPEMD160:
        evp_md = EVP_ripemd160();
        valLen = 20;
        break;

    default:
        exit(1);
    }

    ctx = NULL;
    key = (unsigned char *)malloc(valLen);
    if (!key) exit(1);
    memset(key, 0, valLen);
}

AFK_HMAC_RNG::~AFK_HMAC_RNG()
{
    if (key) free(key);
    if (ctx) delete ctx;
}

void AFK_HMAC_RNG::seed_internal(const AFK_RNG_Value& seed)
{
    memcpy(key, &seed.v.b[0], valLen);
    if (valLen == 32) memcpy(key+16, &seed.v.b[0], 16);
    counter = 0;

    if (ctx)
    {
        HMAC_CTX_cleanup(ctx);
        free(ctx);
    }

    ctx = (HMAC_CTX *)malloc(sizeof(HMAC_CTX));
    if (!ctx) exit(1);
    HMAC_CTX_init(ctx);
    bool success = HMAC_Init_ex(ctx, key, valLen, evp_md, NULL);
    if (!success) exit(1);
}

AFK_RNG_Value AFK_HMAC_RNG::rand(void)
{
    AFK_RNG_Value val;
    unsigned int localValLen = valLen;
    int success;

#if 0
    HMAC_CTX_init(&ctx);
    success = HMAC_Init_ex(&ctx, &key.v.b[0], (int)valLen, evp_md, NULL);
    if (success)
    {
        success = HMAC_Update(&ctx, (unsigned char *)&counter, sizeof(unsigned int));
        ++counter;
    }

    if (success)
    {
        success = HMAC_Final(&ctx, &val.v.b[0], &valLen);
    }

    HMAC_CTX_cleanup(&ctx);

    if (success)
    {
        return val;
    }
    else
    {
        exit(1);
    }
#endif

    if (valLen == 32)
    {
        /* I can get two AFK_RNG_Values out of this hash.
         * Only call the hash function every other time;
         * otherwise, just pull the buffered value
         */
        if ((counter & 1) == 0)
        {
            unsigned char hval[32];
            success = HMAC_Update(ctx, (unsigned char *)&counter, sizeof(unsigned int));
            if (success)
                success = HMAC_Final(ctx, &hval[0], &localValLen);

            if (!success) exit(1);

            /* TODO Why am I getting a wildly weird
             * distribution out of this?!
             */
            memcpy(&val.v.b[0], &hval[0], 16);
            memcpy(&buffered.v.b[0], &hval[16], 16);
        }
        else
        {
            val = buffered;
        }

        ++counter;
    }
    else
    {
        success = HMAC_Update(ctx, (unsigned char *)&counter, sizeof(unsigned int));
        ++counter;

        if (success)
            success = HMAC_Final(ctx, &val.v.b[0], &localValLen);

        if (!success) exit(1);
    }

    return val;
}

