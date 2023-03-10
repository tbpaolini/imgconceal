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
static PassBuff *imc_cli_password_input(bool confirm);

// Free the memory of a 'PassBuff' struct
static void imc_cli_password_free(PassBuff *password);

// Get the pointer of the struct needed by the 'argp_parse()' function
const struct argp *restrict imc_cli_get_argp_struct();

// Main callback function for the command line interface
// It receives the user's arguments, then call other parts of the program in order to perform the requested operation.
int imc_cli_parse_options(int key, char *arg, struct argp_state *state);

#endif  // _IMC_CLI_H