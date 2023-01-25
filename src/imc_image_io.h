#ifndef _IMC_IMAGE_IO_H
#define _IMC_IMAGE_IO_H

#include "imc_includes.h"

// Pointers to the bytes that carry the hidden data
typedef uint8_t *carrier_ptr_t;

enum ImageType {IMC_JPEG, IMC_PNG};

// Bytes of the image that carry the hidden data
typedef struct DataCarrier
{
    // Pointers to the carrier bytes of the image
    carrier_ptr_t *bytes;
    size_t lenght;
    
    // Handler for the image
    union {
        struct jpeg_decompress_struct *jpeg;
        void *png;
    } object;
    enum ImageType type;
} DataCarrier;

// Get bytes of a JPEG image that will carry the hidden data
int imc_jpeg_open_carrier(char *path, DataCarrier **output);

#endif  // _IMC_IMAGE_IO_H