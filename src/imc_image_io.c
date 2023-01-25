#include "imc_includes.h"

// Get bytes of a JPEG image that will carry the hidden data
int imc_jpeg_open_carrier(char *path, DataCarrier **output)
{
    // Open the image for reading
    FILE *jpeg_file = fopen(path, "rb");
    if (!jpeg_file) return -1;
    
    // Parse the header from the image
    struct jpeg_decompress_struct jpeg_obj;
    jpeg_create_decompress(&jpeg_obj);
    jpeg_stdio_src(&jpeg_obj, jpeg_file);
    jpeg_read_header(&jpeg_obj, TRUE);

    // Read the DCT coefficients from the image
    jvirt_barray_ptr *jpeg_dct = jpeg_read_coefficients(&jpeg_obj);
}