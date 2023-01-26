#ifndef _IMC_GLOBALS_H
#define _IMC_GLOBALS_H

#include "imc_includes.h"

// Function return codes
#define IMC_SUCCESS             0   // Operation completed successfully
#define IMC_ERR_NO_MEMORY      -1   // No enough memory
#define IMC_ERR_INVALID_PASS   -2   // Password is not valid
#define IMC_ERR_FILE_NOT_FOUND -3   // File does not exist or could not be opened
#define IMC_ERR_FILE_INVALID   -4   // File is not of a supported format

// Store the byte order of the system
extern bool IS_LITTLE_ENDIAN;

#endif  // _IMC_GLOBALS_H