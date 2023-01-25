#ifndef _IMC_IMAGE_IO_H
#define _IMC_IMAGE_IO_H

#include "imc_includes.h"

// Bytes of the image that carry the hidden data
typedef struct DataCarrier
{
    size_t lenght;
    uint8_t *bytes;
} DataCarrier;

#endif  // _IMC_IMAGE_IO_H