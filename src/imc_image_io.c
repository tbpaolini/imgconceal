#include "imc_includes.h"

// Initialize an image for hiding data in it
int imc_steg_init(const char *path, const char *password, CarrierImage **output)
{
    FILE *image = fopen(path, "rb");
    if (image == NULL) return IMC_ERR_FILE_NOT_FOUND;

    // The file should start with one of these sequences of bytes
    static const uint8_t JPEG_MAGIC[] = {0xFF, 0xD8, 0xFF};
    static const uint8_t PNG_MAGIC[]  = {0x89, 0x50, 0x4E, 0x47};

    // Get the file signature
    const size_t sig_size = 4;
    uint8_t img_marker[sig_size];
    size_t read_count = fread(img_marker, 1, sig_size, image);
    if (read_count != sig_size)
    {
        fclose(image);
        return IMC_ERR_FILE_INVALID;
    }
    fseek(image, 0, SEEK_SET);

    // Determine the image format
    enum ImageType img_type;
    
    if (memcmp(img_marker, JPEG_MAGIC, sizeof(JPEG_MAGIC)) == 0)
    {
        img_type = IMC_JPEG;
    }
    else if (memcmp(img_marker, PNG_MAGIC, sizeof(PNG_MAGIC)) == 0)
    {
        img_type = IMC_PNG;
    }
    else
    {
        fclose(image);
        return IMC_ERR_FILE_INVALID;
    }

    // Holds the information needed for hiding data in the image
    CarrierImage *carrier_img = imc_calloc(1, sizeof(CarrierImage));
    carrier_img->type = img_type;
    carrier_img->file = image;

    // Generate a secret key, and seed the number generator
    imc_crypto_context_create(password, &carrier_img->crypto);

    // Set the struct's methods
    switch (img_type)
    {
        case IMC_JPEG:
            carrier_img->open  = &imc_jpeg_carrier_open;
            carrier_img->close = &imc_jpeg_carrier_close;
            break;
        
        case IMC_PNG:
            /* code */
            break;
    }
    
    // Get the carrier bytes from the image
    carrier_img->open(carrier_img);
    
    *output = carrier_img;
    return IMC_SUCCESS;
}

// Get bytes of a JPEG image that will carry the hidden data
void imc_jpeg_carrier_open(CarrierImage *carrier_img)
{
    // Open the image for reading
    FILE *jpeg_file = carrier_img->file;
    struct jpeg_decompress_struct *jpeg_obj = imc_malloc(sizeof(struct jpeg_decompress_struct));
    struct jpeg_error_mgr *jpeg_err = imc_malloc(sizeof(struct jpeg_error_mgr));
    jpeg_obj->err = jpeg_std_error(jpeg_err);   // Use the default error handler
    jpeg_create_decompress(jpeg_obj);
    
    // Read the DCT coefficients from the image
    jpeg_stdio_src(jpeg_obj, jpeg_file);
    jpeg_read_header(jpeg_obj, true);
    jvirt_barray_ptr *jpeg_dct = jpeg_read_coefficients(jpeg_obj);

    // Calculate the total amount of DCT coeficients
    size_t dct_count = 0;
    for (int comp = 0; comp < jpeg_obj->num_components; comp++)
    {
        dct_count += jpeg_obj->comp_info[comp].height_in_blocks * jpeg_obj->comp_info[comp].width_in_blocks * DCTSIZE2;
    }

    // Estimate the size of the array of carrier values and allocate it
    size_t carrier_capacity = dct_count / 8;
    if (carrier_capacity == 0) carrier_capacity = 1;
    carrier_ptr_t *carrier_ptr = imc_calloc(carrier_capacity, sizeof(uint8_t *));
    size_t carrier_index = 0;
    
    // Iterate over the color components
    for (int comp = 0; comp < jpeg_obj->num_components; comp++)
    {
        // Iterate row by row from from top to bottom
        for (JDIMENSION y = 0; y < jpeg_obj->comp_info[comp].height_in_blocks; y++)
        {
            // Array of DCT coefficients for the current color component
            JBLOCKARRAY coef_array = jpeg_obj->mem->access_virt_barray(
                (j_common_ptr)jpeg_obj,     // Pointer to the JPEG object
                jpeg_dct[comp],             // DCT coefficients for the current color component
                y,                          // The current row of DCT blocks on the image
                1,                          // Read one row of DCT blocks at a time
                true                        // The array should be writeable
            );

            // Iterate column by column from left to right
            for (JDIMENSION x = 0; x < jpeg_obj->comp_info[comp].width_in_blocks; x++)
            {
                // Iterate over the 63 AC coefficients (the DC coefficient is skipped)
                for (JCOEF i = 1; i < DCTSIZE2; i++)
                {
                    // Resize the array of carriers if it is full
                    if (carrier_index == carrier_capacity)
                    {
                        carrier_capacity *= 2;
                        carrier_ptr = imc_realloc(carrier_ptr, carrier_capacity * sizeof(uint8_t *));
                    }

                    // The current coefficient
                    const JCOEF coef = coef_array[0][x][i];
                    uint8_t *const coef_bytes = (uint8_t *)(&coef_array[y][x][i]);

                    // Only the AC coefficients that are not 0 or 1 are used as carriers
                    if (coef != 0 && coef != 1)
                    {
                        carrier_ptr[carrier_index++] = IS_LITTLE_ENDIAN ? &coef_bytes[0] : &coef_bytes[sizeof(JCOEF)-1];
                    }
                }
            }
        }
    }
    
    // Free the unusued space of the array
    carrier_ptr = imc_realloc(carrier_ptr, carrier_index);

    // Store the output
    carrier_img->carrier = carrier_ptr;             // Array of pointers to bytes
    carrier_img->carrier_lenght = carrier_index;    // Total amount of pointers to bytes
    carrier_img->object = jpeg_obj;                 // Image handler
    
    // Store the additional heap allocated memory for the purpose of garbage collection
    carrier_img->heap = imc_malloc(sizeof(void *));
    carrier_img->heap[0] = (void *)jpeg_err;
    carrier_img->heap_lenght = 1;
}

// Free the memory of the data structures used for data hiding
void imc_steg_finish(CarrierImage *carrier_img)
{
    carrier_img->close(carrier_img);
    fclose(carrier_img->file);
    imc_crypto_context_destroy(carrier_img->crypto);
    imc_free(carrier_img);
}

// Free the memory of the array of heap pointers in a CarrierImage struct
static void __carrier_heap_free(CarrierImage *carrier_img)
{
    for (size_t i = 0; i < carrier_img->heap_lenght; i++)
    {
        imc_free(carrier_img->heap[i]);
    }
    free(carrier_img->heap);
}

// Close the JPEG object and free the memory associated to it
void imc_jpeg_carrier_close(CarrierImage *carrier_img)
{
    jpeg_destroy((j_common_ptr)carrier_img->object);
    imc_free(carrier_img->carrier);
    imc_free(carrier_img->object);
    __carrier_heap_free(carrier_img);
}