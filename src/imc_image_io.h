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
    void **heap;            // Array of pointers to other heap allocated memory for this image
    size_t heap_lenght;     // Amount of elements on the 'heap' array
} CarrierImage;

// Initialize an image for hiding data in it
int imc_steg_init(const char *path, const char *password, CarrierImage **output);

// Get bytes of a JPEG image that will carry the hidden data
void imc_jpeg_open_carrier(CarrierImage *output);

// Free the memory of the data structures used for data hiding
void imc_steg_finish(CarrierImage *carrier_img);

// Free the memory of the array of heap pointers in a CarrierImage struct
static void __carrier_heap_free(CarrierImage *carrier_img);

// Close the JPEG object and free the memory associated to it
void imc_jpeg_close_carrier(CarrierImage *carrier_img);

#endif  // _IMC_IMAGE_IO_H