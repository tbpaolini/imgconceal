/* Libraries included in the project */

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
#include <errno.h>

// System libraries
#ifdef _WIN32
#include <windows.h>    // Microsoft Windows API
#endif // _WIN32
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <libgen.h>     // For the basename() function
#include <endian.h>
#include <fcntl.h>      // For the AT_FDCWD macro
#include <termios.h>    // For temporarily turning off input echoing in the terminal
#include <argp.h>       // Command line interface

// Third party libraries
#include <sodium.h>     // libsodium (cryptography)
#include <jpeglib.h>    // libjpeg-turbo (JPEG images)
#include <png.h>        // libpng (PNG images)
#include <zlib.h>       // data compression
#include "../lib/shishua-sse2.h"    // Psueudo-random number generator

// First party libraries
#include "globals.h"
#include "imc_cli.h"
#include "imc_crypto.h"
#include "imc_image_io.h"
#include "imc_memory.h"

#endif  // _IMC_INCLUDES_H