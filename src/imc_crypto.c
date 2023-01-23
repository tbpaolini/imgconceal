#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sodium.h>
#include "imc_crypto.h"

// Parameters for generating a secret key
static const char IMC_SALT[crypto_pwhash_SALTBYTES+1] = "imageconceal2023";
static bool IS_LITTLE_ENDIAN = true;

// Parameters of the Blum Blum Shub algorithm (pseudorandom number generator)
static const uint64_t PRIME_1 = 2147483647UL;
static const uint64_t PRIME_2 = 4294967291UL;
static const uint64_t BBS_MOD = PRIME_1 * PRIME_2;

// Generate a secret key from a password
int imc_crypto_context_create(char *password, CryptoContext **out)
{
    // Verify whether the system is little or big endian
    static bool endianness_test = false;
    if (!endianness_test)
    {
        const uint16_t val = 1;
        const uint8_t *bytes = (uint8_t *)(&val);
        IS_LITTLE_ENDIAN = bytes[0] == 1 ? true : false;
        endianness_test = true;
    }
    
    // Storage for the secret key and the seed of the number generator
    CryptoContext *context = sodium_malloc(sizeof(CryptoContext));
    if (!context) return IMC_ERR_NO_MEMORY;
    
    // Storage for the password hash
    const size_t out_len = sizeof(context->xcc20_key) + sizeof(context->bbs_seed);
    uint8_t output[out_len];
    sodium_mlock(output, sizeof(output));
    
    do
    {
        // Password hashing: generate enough bytes for both the key and the seed
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
        if (status < 0) return IMC_ERR_NO_MEMORY;

        // The lower bytes are used for the key (32 bytes)
        memcpy(&context->xcc20_key, &output[0], sizeof(context->xcc20_key));

        // The upper bytes are used for the seed (64-bit unsigned integer)
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
    } while (
        // Check if the generated seed meets the requirements of the Blum Blum Shub algorithm
        // (all the bellow checks must evaluate to false)
           context->bbs_seed % PRIME_1 == 0
        || context->bbs_seed % PRIME_2 == 0
        || context->bbs_seed <= 1
    );
    
    // Release the unecessary memory and store the output
    sodium_munlock(output, sizeof(output));
    *out = context;

    return IMC_SUCCESS;
}