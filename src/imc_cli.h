/* Command-line interface for imgconceal */

#ifndef _IMC_CLI_H
#define _IMC_CLI_H

#include "imc_includes.h"

// Get a password from the user on the command-line. The typed characters are not displayed.
// They are stored on the 'output' buffer, up to 'buffer_size' bytes.
// Function returns the amount of bytes in the password.
size_t imc_get_password(uint8_t *output, const size_t buffer_size);

#endif  // _IMC_CLI_H