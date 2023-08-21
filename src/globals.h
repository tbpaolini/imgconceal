/* Macro definitions meant for the whole program */

#ifndef _IMC_GLOBALS_H
#define _IMC_GLOBALS_H

#include "imc_includes.h"

// Versions of the data structures (for the purpose of backwards compatibility)
// These values should be positive integers and increase whenever their respective structure changes.
#define IMC_CRYPTO_VERSION      1   // Encrypted stream of the hidden file
#define IMC_FILEINFO_VERSION    2   // Metadata stored inside the encrypted stream

/* Changelog of the data structures:

    IMC_CRYPTO_VERSION:
        1 - Initial version

    IMC_FILEINFO_VERSION:
        1 - Initial version
        2 - Added option for not compressing the hidden data
*/

// Function return codes
#define IMC_SUCCESS             0   // Operation completed successfully
#define IMC_ERR_NO_MEMORY      -1   // No enough memory
#define IMC_ERR_INVALID_PASS   -2   // Password is not valid
#define IMC_ERR_FILE_NOT_FOUND -3   // File does not exist or could not be opened
#define IMC_ERR_FILE_INVALID   -4   // File is not of a supported format
#define IMC_ERR_FILE_TOO_BIG   -5   // The file to be hidden does not fit in the carrier bits of the image
#define IMC_ERR_CRYPTO_FAIL    -6   // Failed to encrypt or decrypt the data
#define IMC_ERR_FILE_EXISTS    -7   // Output file's name already exists
#define IMC_ERR_PAYLOAD_OOB    -8   // Out-of-bounds: attempted to read more hidden data than what is left of the image
#define IMC_ERR_INVALID_MAGIC  -9   // The "magic bytes" of the hidden data did not match what were expected
#define IMC_ERR_NEWER_VERSION  -10  // Data was hidden using a newer version of this program
#define IMC_ERR_SAVE_FAIL      -11  // Failed to save the extracted file
#define IMC_ERR_NAME_TOO_LONG  -12  // The file name has more characters than the maximum allowed
#define IMC_ERR_FILE_CORRUPTED -13  // The file read has a different size than expected
#define IMC_ERR_PATH_IS_DIR    -14  // The path is of a directory rather than a file
#define IMC_ERR_CODEC_FAIL     -15  // Failed to decode or encode an image

// Maximum size in bytes of the file being hidden
#define IMC_MAX_INPUT_SIZE  500000000

// Maximum number that can be appended to a filename in order to resolve name collisions
#define IMC_MAX_FILENAME_DUPLICATES 99

#endif  // _IMC_GLOBALS_H