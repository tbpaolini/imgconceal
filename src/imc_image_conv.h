/* Utilities for converting one image format to another. */

#ifndef _IMC_IMAGE_CONV_H
#define _IMC_IMAGE_CONV_H

#include "imc_includes.h"

// Opaque data structure for storing the image's color values and metadata
struct RawImage;

/*  "Public" functions  */

// Convert an image file to another format
// The converted image is returned as a temporary file, which is automatically
// deleted when it is closed or the program exits.
// In case of failure, NULL is returned and 'imc_codec_error_msg' is set.
FILE *restrict imc_image_convert(FILE *restrict in_file, enum ImageType in_format, enum ImageType out_format);

/*  "Private" functions  */

// Allocate the memory for the color buffer inside a RawImage struct
// Note: the struct members 'height' and 'stride' must have been set previously.
static void __alloc_color_buffer(struct RawImage *raw_image);

#endif  // _IMC_IMAGE_CONV_H