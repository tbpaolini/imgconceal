/* Utilities for converting one image format to another. */

#include "imc_includes.h"

// Buffer for storing arbitrary data
typedef struct RawBuffer {
    size_t size;    // Size in bytes of the buffer
    uint8_t *data;  // Array of bytes
} RawBuffer;

// Store the color values and metadata of an image
// Note: The color values are stored a sequence of four 8-bit channels in the RGBA order (red, green, blue, alpha).
//       If the image does not have transparency, alpha will always be 255.
typedef struct RawImage {
    size_t width;   // Amount of pixels in a row
    size_t height;  // Amount of rows
    size_t stride;  // Amount of bytes per row
    bool has_transparency;  // If the image has transparency
    struct RawBuffer icc;   // Color profile
    struct RawBuffer xmp;   // XMP metadata
    struct RawBuffer exif;  // EXIF metadata
    struct RawBuffer rgba;  // Color values of the pixels: {R1, G1, B1, A1, R2, G2, B2, A2, ...}
    uint8_t **row_pointers; // Pointers to each row on the buffer ofcolor values
} RawImage;

// Convert an image file to another format
// The converted image is returned as a temporary file, which is automatically
// deleted when it is closed or the program exits.
// In case of failure, NULL is returned and 'imc_codec_error_msg' is set.
FILE *restrict imc_image_convert(FILE *restrict in_file, enum ImageType in_format, enum ImageType out_format)
{
    
}

// Allocate the memory for the color buffer inside a RawImage struct
// The memory should be freed with '__free_color_buffer()'.
// Note: the struct members 'height' and 'stride' must have been set previously.
static void __alloc_color_buffer(struct RawImage *raw_image)
{
    const size_t height = raw_image->height;    // Amount of rows
    const size_t stride = raw_image->stride;    // Bytes per row
    const size_t total_size = stride * height;  // Total amount of bytes

    // Allocate the RGBA color buffer
    raw_image->rgba = (RawBuffer){
        .size = total_size,
        .data = imc_malloc(total_size),
    };

    // Pointers to each row on the RGBA color buffer
    raw_image->row_pointers = imc_malloc(height * sizeof(uint8_t*));
    for (size_t i = 0; i < height; i++)
    {
        raw_image->row_pointers[i] = raw_image->rgba.data + (i * stride);
    }
}

// Free the memory for the color buffer inside a RawImage struct
static void __free_color_buffer(struct RawImage *raw_image)
{
    imc_free(raw_image->rgba.data);
    imc_free(raw_image->row_pointers);
}
