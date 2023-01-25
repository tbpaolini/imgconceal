#include "imc_includes.h"

// Get bytes of a JPEG image that will carry the hidden data
int imc_jpeg_open_carrier(char *path, DataCarrier **output)
{
    // Open the image for reading
    FILE *jpeg_file = fopen(path, "rb");
    if (!jpeg_file) return -1;
    struct jpeg_decompress_struct jpeg_obj;
    jpeg_create_decompress(&jpeg_obj);
    
    // Read the DCT coefficients from the image
    jpeg_stdio_src(&jpeg_obj, jpeg_file);
    jpeg_read_header(&jpeg_obj, TRUE);
    jvirt_barray_ptr *jpeg_dct = jpeg_read_coefficients(&jpeg_obj);

    // Calculate the total amount of DCT coeficients
    size_t dct_count = 0;
    for (int comp = 0; comp < jpeg_obj.num_components; comp++)
    {
        dct_count += jpeg_obj.comp_info[comp].height_in_blocks * jpeg_obj.comp_info[comp].width_in_blocks * DCTSIZE2;
    }

    // Estimate the size of the array of carrier values and allocate it
    size_t carrier_capacity = dct_count / 8;
    if (carrier_capacity == 0) carrier_capacity = 1;
    JCOEFPTR *carrier_ptr = calloc(carrier_capacity, sizeof(JCOEF));
    size_t carrier_index = 0;
    
    // Iterate over the color components
    for (int comp = 0; comp < jpeg_obj.num_components; comp++)
    {
        // Array of DCT coefficients for the current color component
        JBLOCKARRAY coef_array = jpeg_obj.mem->access_virt_barray(
            (j_common_ptr)&jpeg_obj,    // Pointer to the JPEG object
            jpeg_dct[comp],             // DCT coefficients for the current color component
            0,                          // The first row of DCT blocks on the image
            jpeg_obj.comp_info[comp].height_in_blocks,  // How many rows of DCT blocks in the image
            true                        // The array should be writeable
        );

        // Iterate row by row from from top to bottom
        for (JDIMENSION y = 0; y < jpeg_obj.comp_info[comp].height_in_blocks; y++)
        {
            // Iterate column by column from left to right
            for (JDIMENSION x = 0; x < jpeg_obj.comp_info[comp].width_in_blocks; x++)
            {
                // Iterate over the 63 AC coefficients (the DC coefficient is skipped)
                for (JCOEF i = 1; i < DCTSIZE2; i++)
                {
                    // Resize the array of carriers if it is full
                    if (carrier_index == carrier_capacity)
                    {
                        carrier_capacity *= 2;
                        carrier_ptr = realloc(carrier_ptr, carrier_capacity * sizeof(JCOEF));
                    }

                    // The current coefficient
                    const JCOEF coef = coef_array[y][x][i];
                    const JCOEFPTR coef_ptr = &coef_array[y][x][i];

                    // Only the AC coefficients that are greater than 1 are used as carriers
                    if (coef > 1)
                    {
                        carrier_ptr[carrier_index++] = coef_ptr;
                    }
                }
            }
        }
    }
    
    // Free the unusued space of the array
    carrier_ptr = realloc(carrier_ptr, carrier_index);
}