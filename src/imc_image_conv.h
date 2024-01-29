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
// Optionally, progress can be printed to stdout by setting the 'verbose' argument to 'true'.
FILE *imc_image_convert(FILE *restrict in_file, enum ImageType in_format, enum ImageType out_format, bool verbose);

#endif  // _IMC_IMAGE_CONV_H