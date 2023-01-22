#include <stdint.h>
#include <string.h>
#include <sodium.h>
#include "imc_crypto.h"

static const char IMC_SALT[crypto_pwhash_SALTBYTES+1] = "imageconceal2023";

// Generate a secret key from a password
int imc_crypto_context_create(char *password, CryptoContext **out)
{
    CryptoContext *context = sodium_malloc(sizeof(CryptoContext));
    if (!context) return IMC_ERR_NO_MEMORY;
    
    uint8_t output[sizeof(CryptoContext)];
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

    *context = (CryptoContext){
        .xcc20_key = *(uint64_t*)(&output[0]),
        .bbs_seed  = *(uint64_t*)(&output[sizeof(uint64_t)])
    };
    
    sodium_munlock(output, sizeof(output));

    *out = context;
    return 0;
}