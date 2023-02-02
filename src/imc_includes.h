#ifndef _IMC_INCLUDES_H
#define _IMC_INCLUDES_H

// Standard libraries
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <locale.h>
#include <time.h>
#include <ctype.h>

// System libraries
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <libgen.h>     // For the basename() function
#include <endian.h>
#include <fcntl.h>      // For the AT_FDCWD macro

// Third party libraries
#include <sodium.h>     // libsodium (cryptography)
#include <jpeglib.h>    // libjpeg-turbo (JPEG images)
#include <zlib.h>       // data compression

// First party libraries
#include "globals.h"
#include "imc_crypto.h"
#include "imc_image_io.h"
#include "imc_memory.h"

#endif  // _IMC_INCLUDES_H