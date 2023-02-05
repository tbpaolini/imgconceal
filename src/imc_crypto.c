#include "imc_includes.h"
#include "imc_crypto_primes.h"

// Header for encrypting the data stream
static unsigned char IMC_HEADER[crypto_secretstream_xchacha20poly1305_HEADERBYTES+1] = "imageconceal v1.0.0";

// Generate cryptographic secrets key from a password
int imc_crypto_context_create(const char *password, CryptoContext **out)
{
    // Salt for generating a secret key from a password
    uint8_t salt[crypto_pwhash_SALTBYTES];
    memset(salt, 0, sizeof(salt));
    size_t salt_len = strlen(IMC_SALT);
    if (salt_len > crypto_pwhash_SALTBYTES) salt_len = crypto_pwhash_SALTBYTES;
    memcpy(salt, IMC_SALT, salt_len);
    
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
            salt,
            IMC_OPSLIMIT,
            (cycle == 0) ? IMC_MEMLIMIT : IMC_MEMLIMIT_REHASH,
            crypto_pwhash_ALG_ARGON2ID13
        );
        if (status < 0) return IMC_ERR_NO_MEMORY;

        // The lower bytes are used for the key (32 bytes)
        memcpy(&context->xcc20_key, &output[0], key_size);

        // The upper bytes are used for the seed (64-bit unsigned integer)
        // and for choosing the primes for the BBS algorithm
        
        // Seed's bytes (4 bytes)
        memcpy(&context->bbs_seed, &output[key_size], seed_size);
        context->bbs_seed = le64toh(context->bbs_seed);

        // Primes' indexes (2 bytes for each)
        const size_t pos = key_size + seed_size;
        p[0] = le16toh( *(uint16_t*)(&output[pos]) );
        p[1] = le16toh( *(uint16_t*)(&output[pos+2]) );

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
// It writes a given amount of bytes to the output.
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
        output[i] = byte;
    }
}

// Generate an unsigned 64-bit integer that can be up to the 'max' value (inclusive)
uint64_t imc_crypto_prng_uint64(CryptoContext *state, uint64_t max)
{
    if (max == UINT64_MAX)
    {
        uint64_t random_num;
        imc_crypto_prng(state, sizeof(uint64_t), (uint8_t *)&random_num);
        return le64toh(random_num); // Invert the byte order on big endian systems
    }
    
    // Ensure that the 'max' value is included in the range
    max += 1;
    
    // Get the least amount of bytes needed to represent the amount of elements
    const uint64_t num_bits = ceil( log2( (double)(max + 1UL) ) );
    const uint64_t num_bytes = (num_bits % 8 == 0) ? (num_bits / 8) : (num_bits / 8) + 1;
    
    // Ensure that the random values will be evenly distributed in the range of '0' to 'num_elements - 1'
    const uint64_t max_random = (1 << (num_bytes * 8)) - 1;
    const uint64_t cutoff = max_random - (max_random % max);

    // Generate a pseudorandom number
    uint64_t random_num;
    do
    {
        random_num = 0;
        imc_crypto_prng(state, num_bytes, (uint8_t *)&random_num);
        random_num = le64toh(random_num);   // Invert the byte order on big endian systems
    } while (random_num > cutoff);

    return random_num % max;
}

// Randomize the order of the elements in an array of pointers
void imc_crypto_shuffle_ptr(CryptoContext *state, uintptr_t *array, size_t num_elements)
{
    if (num_elements <= 1) return;
    
    // Fisher-Yates shuffle algorithm:
    // Each element 'E[i]' is swapped with a random element of index smaller or equal than 'i'.
    // Explanation of why not just swapping by any other element: https://blog.codinghorror.com/the-danger-of-naivete/
    for (size_t i = num_elements-1; i > 0; i--)
    {
        // A pseudorandom index smaller or equal than the current index
        size_t new_i = imc_crypto_prng_uint64(state, i);
        if (new_i == i) continue;

        // Swap the current element with the element on the element on the random index
        array[i] ^= array[new_i];
        array[new_i] ^= array[i];
        array[i] ^= array[new_i];
    }
}

// Encrypt a data stream
int imc_crypto_encrypt(
    CryptoContext *state,
    const uint8_t *const data,
    unsigned long long data_len,
    uint8_t *output,
    unsigned long long *output_len
)
{
    // Initialize the encryption
    crypto_secretstream_xchacha20poly1305_state encryption_state;
    int status = crypto_secretstream_xchacha20poly1305_init_push(
        &encryption_state,
        IMC_HEADER,
        state->xcc20_key
    );

    if (status < 0) return status;

    // Encrypt the data
    status = crypto_secretstream_xchacha20poly1305_push(
        &encryption_state,  // Parameters for encryption
        &output[IMC_HEADER_OVERHEAD],   // Output buffer for the ciphertext (skip the space for the metadata that we are going to prepend)
        output_len,         // Stores the amount of bytes written to the output buffer
        data,               // Data to be encrypted
        data_len,           // Size in bytes of the data
        NULL,               // Additional data (nothing in our case)
        0,                  // Size in bytes of the additional data
        crypto_secretstream_xchacha20poly1305_TAG_FINAL // Tag that this is the last data of the stream
    );

    if (status < 0) return status;

    // Pointers to the adresses where the version and size will be written
    uint32_t *version = (uint32_t *)&output[4];
    uint32_t *c_size = (uint32_t *)&output[8];
    
    // Write the metadata to the beginning of the buffer
    memcpy(&output[0], IMC_CRYPTO_MAGIC, 4);             // Add the file signature (magic bytes)
    *version = htole32( (uint32_t)IMC_CRYPTO_VERSION );  // Version of the current encryption process
    *c_size = htole32( (uint32_t)(*output_len) );        // Size of the encrypted stream that follows

    *output_len += IMC_HEADER_OVERHEAD;

    return status;
}

// Decrypt a data stream
int imc_crypto_decrypt(
    CryptoContext *state,
    const uint8_t *const data,
    unsigned long long data_len,
    uint8_t *output,
    unsigned long long *output_len
)
{
    // Initialize the decryption
    crypto_secretstream_xchacha20poly1305_state decryption_state;
    int status = crypto_secretstream_xchacha20poly1305_init_pull(
        &decryption_state,
        IMC_HEADER,
        state->xcc20_key
    );

    if (status < 0) return status;

    unsigned char tag = 0;

    // Decrypt the data
    status = crypto_secretstream_xchacha20poly1305_pull(
        &decryption_state,  // Parameters for decryption
        output,             // Output buffer for the decrypted data
        output_len,         // Size in bytes of the output buffer (on success, the function stores here how many bytes were written)
        &tag,               // Output for the tag attached to the data (in the current version, it should be tagged as FINAL)
        data,               // Input buffer with the encrypted data
        data_len,           // Size in bytes of the input buffer
        NULL,               // Buffer for the additional data (we are not using it)
        0                   // Size of the buffer for additional data
    );

    if (status < 0)
    {
        return status;
    }
    else
    {
        if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL)
        {
            // Decryption worked
            return status;
        }
        else
        {
            // Theoretically, this branch is unreachable because (in this version) the encryption always tags the data as FINAL.
            // But the check for the tag is here "just in case".
            sodium_memzero(output, *output_len);
            return IMC_ERR_CRYPTO_FAIL;
        }
    }
}

// Free the memory used by the cryptographic secrets
void imc_crypto_context_destroy(CryptoContext *state)
{
    sodium_free(state);
}