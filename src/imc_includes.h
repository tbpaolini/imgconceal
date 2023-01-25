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

// Third party libraries
#include <sodium.h>     // libsodium (cryptography)
#include <jpeglib.h>    // libjpeg-turbo (JPEG images)
#include <zlib.h>       // data compression

// First party libraries
#include "global.h"
#include "imc_crypto.h"
#include "imc_image_io.h"
#include "imc_memory.h"

#endif  // _IMC_INCLUDES_H