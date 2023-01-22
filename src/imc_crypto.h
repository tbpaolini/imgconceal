#ifndef _IMC_CRYPTO_H
#define _IMC_CRYPTO_H

#include <stdint.h>
#include <sodium.h>

// Password hashing parameters
#define IMC_OPSLIMIT 3          // Amount of operations
#define IMC_MEMLIMIT 4096000    // Amount of memory

// Error codes
#define IMC_SUCCESS           0 // No error
#define IMC_ERR_NO_MEMORY    -1 // No enough memory
#define IMC_ERR_INVALID_PASS -2 // Password is not valid

// Stores the secret key for encryption and the seed of the pseudorandom number generator
typedef struct CryptoContext
{
    uint64_t xcc20_key;
    uint64_t bbs_seed;
} CryptoContext;

// Generate a secret key from a password
int imc_crypto_context_create(char *password, CryptoContext **out);

#endif  // _IMC_CRYPTO_H