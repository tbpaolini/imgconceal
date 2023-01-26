#ifndef _IMC_IMAGE_IO_H
#define _IMC_IMAGE_IO_H

#include "imc_includes.h"

// Pointers to the bytes that carry the hidden data
typedef uint8_t *carrier_ptr_t;

enum ImageType {IMC_JPEG, IMC_PNG};

// Image that will carry the hidden data
typedef struct CarrierImage
{
    FILE *file;             // File ponter of the image
    carrier_ptr_t *carrier; // Array of pointers to the carrier bytes of the image
    size_t carrier_lenght;  // Amount of carrier bytes
    void* object;           // Pointer to the handler that should be passed to the image processing functions
    CryptoContext *crypto;  // Secret parameters generated from the password
    enum ImageType type;    // Format of the image
} CarrierImage;

// Get bytes of a JPEG image that will carry the hidden data
int imc_jpeg_open_carrier(char *path, CarrierImage **output);

#endif  // _IMC_IMAGE_IO_H