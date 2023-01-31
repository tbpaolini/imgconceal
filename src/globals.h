#ifndef _IMC_GLOBALS_H
#define _IMC_GLOBALS_H

#include "imc_includes.h"

// Versions of the data structures (for the purpose of backwards compatibility)
// These values should be positive integers and increase whenever their respective structure changes.
#define IMC_CRYPTO_VERSION      1   // Encrypted stream of the hidden file
#define IMC_FILEINFO_VERSION    1   // Metadata stored inside the encrypted stream

// Function return codes
#define IMC_SUCCESS             0   // Operation completed successfully
#define IMC_ERR_NO_MEMORY      -1   // No enough memory
#define IMC_ERR_INVALID_PASS   -2   // Password is not valid
#define IMC_ERR_FILE_NOT_FOUND -3   // File does not exist or could not be opened
#define IMC_ERR_FILE_INVALID   -4   // File is not of a supported format
#define IMC_ERR_FILE_TOO_BIG   -5   // The file to be hidden does not fit in the carrier bits of the image
#define IMC_ERR_CRYPTO_FAIL    -6   // Failed to encrypt or decrypt the data

// Maximum size in bytes of the file being hidden
#define IMC_MAX_INPUT_SIZE  500000000

#endif  // _IMC_GLOBALS_H