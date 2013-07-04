/* AFK (c) Alex Holloway 2013 */

#include <stdlib.h>
#include <string.h>

#include "aes.hpp"

AFK_AES_RNG::AFK_AES_RNG()
{
    evp_cipher = EVP_aes_128_cbc();
    ctx = NULL;
}

AFK_AES_RNG::~AFK_AES_RNG()
{
    if (ctx) delete ctx;
}

void AFK_AES_RNG::seed_internal(const AFK_RNG_Value& seed)
{
    if (ctx)
    {
        EVP_CIPHER_CTX_cleanup(ctx);
        free(ctx);
    }

    ctx = (EVP_CIPHER_CTX *)malloc(sizeof(EVP_CIPHER_CTX));
    if (!ctx) exit(1);
    EVP_CIPHER_CTX_init(ctx);

    /* TODO better IV */
    if (!EVP_EncryptInit_ex(ctx, evp_cipher, NULL, &key.v.b[0], &key.v.b[0])) exit(1);

    /* Dump the first block (which will have the IV) */
    rand();
}

AFK_RNG_Value AFK_AES_RNG::rand(void)
{
    AFK_RNG_Value val;
    int success;
#if 0
    unsigned char cipher[32];
    int valLen = sizeof(counter);
    int cipherLen = sizeof(cipher);

    /* TODO better IV */
    success = EVP_EncryptInit_ex(ctx, evp_cipher, NULL, &key.v.b[0], &key.v.b[0]);
    if (success)
        success = EVP_EncryptUpdate(ctx, cipher, &cipherLen, (unsigned char *)&counter[0], valLen);

    if (success)
        success = EVP_EncryptFinal(ctx, cipher + cipherLen, &cipherLen);

    if (success)
        memcpy(&val.v.b[0], &cipher[16], 16);
#else
    int valLen = sizeof(counter);
    int cipherLen = 16;

    /* Pull just one block (actually the ciphertext of
     * the last counter value to be put in, not this one)
     */
    success = EVP_EncryptUpdate(ctx, &val.v.b[0], &cipherLen, (unsigned char *)&counter[0], valLen);
#endif

    ++(counter[0]);
    if (!counter[0]) ++(counter[1]);
    if (!counter[1]) ++(counter[2]);
    if (!counter[2]) ++(counter[3]);

    if (!success) exit(1);
    return val;
}

