#ifndef _IMC_IMAGE_IO_H
#define _IMC_IMAGE_IO_H

#include "imc_includes.h"

// Bytes of the image that carry the hidden data
typedef struct DataCarrier
{
    size_t lenght;
    uint8_t *bytes;
} DataCarrier;

// Get bytes of a JPEG image that will carry the hidden data
int imc_jpeg_open_carrier(char *path, DataCarrier **output);

#endif  // _IMC_IMAGE_IO_H