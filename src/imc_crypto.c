#include "imc_includes.h"
#include "imc_crypto_primes.h"

// Parameter for generating a secret key
static const char IMC_SALT[crypto_pwhash_SALTBYTES+1] = "imageconceal2023";

// Generate cryptographic secrets key from a password
int imc_crypto_context_create(const char *password, CryptoContext **out)
{
    // Storage for the secret key and the seed of the number generator
    CryptoContext *context = sodium_malloc(sizeof(CryptoContext));
    sodium_memzero(context, sizeof(CryptoContext));
    if (!context) return IMC_ERR_NO_MEMORY;
    
    // Storage for the password hash
    // Note: Only the lower half of the seed bytes will be filled,
    //       in order to ensure it will not overflow when squared.
    const size_t key_size = sizeof(context->xcc20_key);
    const size_t seed_size = sizeof(context->bbs_seed) / 2;
    const size_t out_len = key_size + seed_size + 4;
    uint8_t output[out_len];
    uint8_t previous_output[out_len];
    sodium_mlock(output, sizeof(output));
    sodium_mlock(previous_output, sizeof(output));
    
    // Primes for the Blum Blum Shub pseudorandom number generator
    // (see the comments at the file 'imc_crypto_primes.h')
    const uint16_t bbs_primes[] = bbs_primes_def;
    
    // Indexes of two primes on 'bbs_primes'
    uint16_t p[2];
    sodium_mlock(p, sizeof(p));

    // How many times were attempted to calculate the hash
    size_t cycle = 0;
    
    do
    {
        if (cycle > 0) memcpy(previous_output, output, sizeof(output));
        
        // Password hashing: generate enough bytes for both the key and the seed
        // On the first cycle, we just hash the password.
        // If it failed to generate a suitable hash, on the next cycle we hash the previous hash.
        int status = crypto_pwhash(
            (uint8_t * const)&output,
            sizeof(output),
            (cycle == 0) ? password : (const char * const)previous_output,
            (cycle == 0) ? strlen(password) : sizeof(output),
            IMC_SALT,
            IMC_OPSLIMIT,
            (cycle == 0) ? IMC_MEMLIMIT : IMC_MEMLIMIT_REHASH,
            crypto_pwhash_ALG_ARGON2ID13
        );
        if (status < 0) return IMC_ERR_NO_MEMORY;

        // The lower bytes are used for the key (32 bytes)
        memcpy(&context->xcc20_key, &output[0], key_size);

        // The upper bytes are used for the seed (64-bit unsigned integer)
        // and for choosing the primes for the BBS algorithm
        if (IS_LITTLE_ENDIAN)
        {
            // Seed's bytes
            memcpy(&context->bbs_seed, &output[key_size], seed_size);

            // Primes' indexes
            const size_t pos = key_size + seed_size;
            memcpy(&p[0], &output[pos], 2);
            memcpy(&p[1], &output[pos+2], 2);
        }
        else
        {
            // Seed's bytes
            uint8_t *seed_buffer = (uint8_t *)(&context->bbs_seed);
            for (size_t i = 0; i < seed_size; i++)
            {
                seed_buffer[i] = output[out_len - 5 - i];
            }

            // Primes' indexes
            uint8_t *p_buffer = (uint8_t*)(&p[0]);
            
            const size_t pos = out_len - 4;
            p_buffer[0] = output[pos+1];
            p_buffer[1] = output[pos+0];
            p_buffer[2] = output[pos+3];
            p_buffer[3] = output[pos+2];
        }

        // Ensure that the indexes should be unbiased when modded by 'bbs_primes_len'
        const uint16_t p_max = UINT16_MAX - (UINT16_MAX % bbs_primes_len);
        if (p[0] > p_max || p[1] > p_max)
        {
            // Force the loop to start again
            p[0] = 0;
            p[1] = 0;
            continue;
        }

        // Ensure that the primes' indexes are within the array bounds
        p[0] %= bbs_primes_len;
        p[1] %= bbs_primes_len;

        // Multiply the two primes to get the value that will be used on the BBS algorithm
        context->bbs_mod = (uint64_t)bbs_primes[p[0]] * (uint64_t)bbs_primes[p[1]];

        cycle++;

    } while (
        // Check if the generated seed meets the requirements of the Blum Blum Shub algorithm
        // (all the checks bellow must evaluate to false)
           p[0] == p[1]
        || context->bbs_seed % context->bbs_mod == 0
        || context->bbs_seed <= 1
    );
    
    // Release the unecessary memory and store the output
    sodium_munlock(output, sizeof(output));
    sodium_munlock(previous_output, sizeof(output));
    sodium_munlock(p, sizeof(p));
    *out = context;

    return IMC_SUCCESS;
}

// Pseudorandom number generator using the Blum Blum Shub algorithm
// It writes a given amount of bytes to the output, while taking into account the endianness of the system.
void imc_crypto_prng(CryptoContext *state, size_t num_bytes, uint8_t *output)
{
    // Bitmasks
    static const uint8_t bit[8] = {1, 2, 4, 8, 16, 32, 64, 128};
    
    for (size_t i = 0; i < num_bytes; i++)
    {
        // Fill a byte with random bits
        uint8_t byte = 0;
        for (size_t j = 0; j < 8; j++)
        {
            // Take the least significant bit of each iteration
            state->bbs_seed = (state->bbs_seed * state->bbs_seed) % state->bbs_mod;
            if (state->bbs_seed & 1) byte |= bit[j];
        }

        // Fill the output with the generated bytes
        if (IS_LITTLE_ENDIAN)
        {
            output[i] = byte;
        }
        else
        {
            output[num_bytes - 1 - i] = byte;
        }
    }
}

// Randomize the order of the elements in an array of pointers
void imc_crypto_shuffle_ptr(CryptoContext *state, uintptr_t *array, size_t num_elements)
{
    if (num_elements <= 1) return;

    // Get the least amount of bytes needed to represent the amount of elements
    const size_t num_bits = ceil( log2( (double)(num_elements + 1UL) ) );
    const size_t num_bytes = (num_bits % 8 == 0) ? (num_bits / 8) : (num_bits / 8) + 1;
    
    // Ensure that the random values will be evenly distributed in the range of '0' to 'num_elements - 1'
    const size_t max_value = (1 << (num_bytes * 8)) - 1;
    const size_t cutoff = max_value - (max_value % num_elements);

    // Iterate over all array's elements and swap them around
    for (size_t i = 0; i < num_elements; i++)
    {
        // Generate a pseudorandom number
        size_t random_num;
        do
        {
            random_num = 0;
            imc_crypto_prng(state, num_bytes, (uint8_t *)&random_num);
        } while (random_num > cutoff);

        // A pseudorandom index that falls inside the array
        size_t new_i = random_num % num_elements;
        if (new_i == i) continue;

        // Swap the current element with the element on the element on the random index
        array[i] ^= array[new_i];
        array[new_i] ^= array[i];
        array[i] ^= array[new_i];
    }
}

// Free the memory used by the cryptographic secrets
void imc_crypto_context_destroy(CryptoContext *state)
{
    sodium_free(state);
}