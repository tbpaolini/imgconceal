#ifndef _IMC_CRYPTO_H
#define _IMC_CRYPTO_H

#include <stdint.h>
#include <sodium.h>

// Stores the secret key for encryption and the seed of the pseudorandom number generator
typedef struct CryptoContext
{
    uint64_t xcc20_key;
    uint64_t bbs_seed;
};
typedef struct CryptoContext imc_key_t;

#endif  // _IMC_CRYPTO_H