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

// Get a password from the user on the command-line. The typed characters are not displayed.
// They are stored on the 'output' buffer, up to 'buffer_size' bytes.
// Function returns the amount of bytes in the password.
static size_t __get_password(uint8_t *output, const size_t buffer_size);

// Allocate memory for a 'PassBuff' struct
static PassBuff *__alloc_passbuff();

// Prompt the user to input a password on the terminal.
// The input's maximum size is determined by the macro IMC_PASSWORD_MAX_BYTES,
// being truncated if that size is reached.
// If 'confirm' is true, the user is asked to type the same password again.
// Function returns NULL if the password confirmation failed.
// The returned 'PassBuff' pointer should be freed with 'imc_cli_password_free()'.
PassBuff *imc_cli_password_input(bool confirm);

// Free the memory of a 'PassBuff' struct
void imc_cli_password_free(PassBuff *password);

#endif  // _IMC_CLI_H