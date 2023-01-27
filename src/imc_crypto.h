#ifndef _IMC_CRYPTO_H
#define _IMC_CRYPTO_H

#include "imc_includes.h"

// Password hashing parameters
#define IMC_OPSLIMIT 3          // Amount of operations
#define IMC_MEMLIMIT 4096000    // Amount of memory

// Stores the secret key for encryption and the seed of the pseudorandom number generator
typedef struct CryptoContext
{
    uint8_t xcc20_key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
    uint64_t bbs_seed;
    uint64_t bbs_mod;
} CryptoContext;

// Generate cryptographic secrets key from a password
int imc_crypto_context_create(const char *password, CryptoContext **out);

// Pseudorandom number generator using the Blum Blum Shub algorithm
// It writes a given amount of bytes to the output, while taking into account the endianness of the system.
void imc_crypto_prng(CryptoContext *state, size_t num_bytes, uint8_t *output);

// Randomize the order of the elements in an array of pointers
void imc_crypto_shuffle_ptr(CryptoContext *state, uintptr_t *array, size_t num_elements);

// Free the memory used by the cryptographic secrets
void imc_crypto_context_destroy(CryptoContext *state);

#endif  // _IMC_CRYPTO_H