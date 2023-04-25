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

// Store on a pointer the full path of a file
static inline void __store_path(const char *path, char **destination);

// Check if an option has not been passed before (program exits if this check fails)
// The idea is to check if the option's value evaluates to 'false'. If it doesn't, then the check fails.
// The error message contains the name of the option, that is why it is needed.
static inline void __check_unique_option(struct argp_state *state, const char *option_name, bool option_value);

// Convert a timespec struct to a date string, and store it on 'out_buff'
static inline void __timespec_to_string(struct timespec *time, char *out_buff, size_t buff_size);

// Convert a file size (in bytes) to a string in the appropriate scale, and store it on 'out_buff'
static inline void __filesize_to_string(size_t file_size, char *out_buff, size_t buff_size);

// Validate the command line options, and perform the requested operation
// This is a helper for the 'imc_cli_parse_options()' function.
static inline void __execute_options(struct argp_state *state, void *options);

// Main callback function for the command line interface
// It receives the user's arguments, then call other parts of the program in order to perform the requested operation.
static int imc_cli_parse_options(int key, char *arg, struct argp_state *state);

// Print a summary of imgconceal's algorithm
static void imc_cli_print_algorithm();

#endif  // _IMC_CLI_H