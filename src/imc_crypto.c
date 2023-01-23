#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sodium.h>
#include "imc_crypto.h"

static const char IMC_SALT[crypto_pwhash_SALTBYTES+1] = "imageconceal2023";
static bool IS_LITTLE_ENDIAN = true;

// Generate a secret key from a password
int imc_crypto_context_create(char *password, CryptoContext **out)
{
    static bool endianess_test = false;
    if (!endianess_test)
    {
        const uint16_t val = 1;
        const uint8_t *bytes = (uint8_t *)(&val);
        IS_LITTLE_ENDIAN = bytes[0] == 1 ? true : false;
        endianess_test = true;
    }
    
    CryptoContext *context = sodium_malloc(sizeof(CryptoContext));
    if (!context) return IMC_ERR_NO_MEMORY;
    
    const size_t out_len = sizeof(context->xcc20_key) + sizeof(context->bbs_seed);
    uint8_t output[out_len];
    sodium_mlock(output, sizeof(output));
    
    int status = crypto_pwhash(
        (uint8_t * const)&output,
        sizeof(output),
        password,
        strlen(password),
        IMC_SALT,
        IMC_OPSLIMIT,
        IMC_MEMLIMIT,
        crypto_pwhash_ALG_ARGON2ID13
    );
    if (status < 0) return IMC_ERR_INVALID_PASS;

    memcpy(&context->xcc20_key, &output[0], sizeof(context->xcc20_key));

    if (IS_LITTLE_ENDIAN)
    {
        memcpy(&context->bbs_seed, &output[sizeof(context->xcc20_key)], sizeof(context->bbs_seed));
    }
    else
    {
        uint8_t *seed_buffer = (uint8_t *)(&context->bbs_seed);
        for (size_t i = 0; i < sizeof(context->bbs_seed); i++)
        {
            seed_buffer[i] = output[out_len - 1 - i];
        }
    }
    
    sodium_munlock(output, sizeof(output));

    *out = context;
    return 0;
}