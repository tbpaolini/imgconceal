/* Command-line interface for imgconceal */

#ifndef _IMC_CLI_H
#define _IMC_CLI_H

#include "imc_includes.h"

#define IMC_PASSWORD_MAX_BYTES 4080     // Size (in bytes) of the password buffer

// Buffer for the plaintext password
typedef struct PassBuff {
    size_t capacity;    // The maximum amount of bytes that the buffer can store
    size_t length;      // The current amount of bytes stored on the buffer
    uint8_t buffer[IMC_PASSWORD_MAX_BYTES]; // Array of bytes with the plaintext password
} PassBuff;

// Get the pointer of the struct needed by the 'argp_parse()' function
const struct argp *imc_cli_get_argp_struct();

#endif  // _IMC_CLI_H