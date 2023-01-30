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
            carrier_img->write = &imc_jpeg_carrier_write;
            carrier_img->close = &imc_jpeg_carrier_close;
            break;
        
        case IMC_PNG:
            carrier_img->open  = &imc_png_carrier_open;
            carrier_img->write = &imc_png_carrier_write;
            carrier_img->close = &imc_png_carrier_close;
            break;
    }
    
    // Get the carrier bytes from the image
    carrier_img->open(carrier_img);
    
    *output = carrier_img;
    return IMC_SUCCESS;
}

// Convenience function for ensuring that the values from the timespec struct are 64-bit
static inline struct timespec64 __timespec_to_64(struct timespec time)
{
    return (struct timespec64){
        .tv_sec  = (int64_t)time.tv_sec,
        .tv_nsec = (int64_t)time.tv_nsec
    };
}

// Hide a file in an image
int imc_steg_insert(CarrierImage *carrier_img, const char *file_path)
{
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) return IMC_ERR_FILE_NOT_FOUND;

    // Get the file's metadata
    int file_descriptor = fileno(file);
    struct stat file_stats;
    fstat(file_descriptor, &file_stats);
    const off_t file_size = file_stats.st_size;
    
    // Sanity check
    if (file_size > IMC_MAX_INPUT_SIZE)
    {
        fprintf(stderr, "Error: Maximum size of the hidden file is 500 MB");
        exit(EXIT_FAILURE);
        /* Note:
            The 500 MB limit is for preventing a huge file from being accidentally loaded.
            Since the amount of data that can realistically be hidden usually is quite small,
            I thought it would be an overkill to optimize the program for handling large files.
        */
    }

    // Get the file name from the path
    const size_t path_len = strlen(file_path);
    char path_temp[path_len+1];
    strcpy(path_temp, file_path);
    char *file_name = basename(path_temp);
    
    // Calculate the size for the file's metadata that will be stored
    const size_t name_size = strlen(file_name) + 1;
    const size_t info_size = sizeof(FileInfo) + name_size;
    
    // Read the file into a buffer
    const size_t buffer_size = info_size + file_size;
    uint8_t *const raw_buffer = imc_malloc(buffer_size);
    const size_t read_count = fread(&raw_buffer[info_size], 1, file_size, file);
    fclose(file);
    if (read_count != file_size) return IMC_ERR_FILE_INVALID;

    // The offset from which the data will be compressed
    const size_t compressed_offset = offsetof(FileInfo, access_time);
    
    // Store the metadata
    FileInfo *file_info = (FileInfo*)raw_buffer;
    file_info->version = IMC_FILEINFO_VERSION;
    file_info->uncompressed_size = buffer_size - compressed_offset;
    file_info->access_time = __timespec_to_64(file_stats.st_atim);
    file_info->mod_time = __timespec_to_64(file_stats.st_mtim);
    file_info->name_size = name_size;
    memcpy(&file_info->file_name[0], file_name, name_size);
    struct timespec current_time;
    clock_gettime(CLOCK_REALTIME, &current_time);
    file_info->steg_time = __timespec_to_64(current_time);

    // Create a buffer for the compressed data
    // Note: For the overhead calculation, see https://zlib.net/zlib_tech.html
    const size_t zlib_overhead = 6 + (5 * (file_size / 16000)) + 1;
    size_t zlib_buffer_size = buffer_size + zlib_overhead;
    uint8_t *const input_buffer = (uint8_t *)(&file_info->access_time);
    uint8_t *zlib_buffer = imc_malloc(zlib_buffer_size);
    
    // Copy the uncompressed metadata to the beginning of the buffer
    memcpy(zlib_buffer, file_info, compressed_offset);
    zlib_buffer_size -= compressed_offset;
    
    // Compress the data on the buffer (from the '.access_time' onwards)
    int status = compress2(
        &zlib_buffer[compressed_offset],    // Output buffer to store the compressed data (starting after the uncompressed section)
        &zlib_buffer_size,                  // Size in bytes of the output buffer (the function updates the value to the used size)
        input_buffer,                       // Data being compressed
        file_info->uncompressed_size,       // Size in bytes of the data
        9                                   // Compression level
    );

    if (status != 0) return IMC_ERR_FILE_TOO_BIG;
    
    // Store the actual size of the compressed data
    ((FileInfo *)zlib_buffer)->compressed_size = zlib_buffer_size;

    imc_free(raw_buffer);

    // Free the unused space in the output buffer
    zlib_buffer = imc_realloc(zlib_buffer, zlib_buffer_size);

    /* TO DO: Encrypt the data */

    imc_free(zlib_buffer);

    /* TO DO: Write the data to the carrier */

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
    carrier_bytes_t carrier_bytes = imc_calloc(carrier_capacity, sizeof(uint8_t));
    size_t carrier_count = 0;
    
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
                false                       // Opening the array in read-only mode
            );

            // Iterate column by column from left to right
            for (JDIMENSION x = 0; x < jpeg_obj->comp_info[comp].width_in_blocks; x++)
            {
                // Iterate over the 63 AC coefficients (the DC coefficient is skipped)
                for (JCOEF i = 1; i < DCTSIZE2; i++)
                {
                    // Resize the array of carriers if it is full
                    if (carrier_count == carrier_capacity)
                    {
                        carrier_capacity *= 2;
                        carrier_bytes = imc_realloc(carrier_bytes, carrier_capacity * sizeof(uint8_t));
                    }

                    // The current coefficient
                    const JCOEF coef = coef_array[0][x][i];
                    const uint8_t *const coef_bytes = (uint8_t *)(&coef);

                    // Only the AC coefficients that are not 0 or 1 are used as carriers
                    if (coef != 0 && coef != 1)
                    {
                        // Store the value of the least significant byte of the coefficient
                        carrier_bytes[carrier_count++] = IS_LITTLE_ENDIAN ? coef_bytes[0] : coef_bytes[sizeof(JCOEF)-1];
                    }
                }
            }
        }
    }
    
    // Free the unusued space of the array
    carrier_bytes = imc_realloc(carrier_bytes, carrier_count * sizeof(uint8_t));

    // Store the pointers to each element of the bytes array
    carrier_bytes_t *carrier_ptr = imc_calloc(carrier_count, sizeof(uint8_t *));

    for (size_t i = 0; i < carrier_count; i++)
    {
        carrier_ptr[i] = &carrier_bytes[i];
    }

    // Shuffle the array of pointers
    // (so the order that the bytes are written depends on the password)
    imc_crypto_shuffle_ptr(
        carrier_img->crypto,
        (uintptr_t *)(&carrier_ptr[0]),
        carrier_count
    );

    // Store the output
    carrier_img->bytes = carrier_bytes;             // Array of bytes
    carrier_img->carrier = carrier_ptr;             // Array of pointers to bytes
    carrier_img->carrier_lenght = carrier_count;    // Total amount of pointers to bytes
    carrier_img->object = jpeg_obj;                 // Image handler
    
    // Store the additional heap allocated memory for the purpose of memory management
    carrier_img->heap = imc_malloc(sizeof(void *));
    carrier_img->heap[0] = (void *)jpeg_err;
    carrier_img->heap_lenght = 1;
}

// Get bytes of a PNG image that will carry the hidden data
void imc_png_carrier_open(CarrierImage *output)
{

}

// Hide data in a JPEG image
int imc_jpeg_carrier_write(CarrierImage *carrier_img, uint8_t *data, size_t data_len)
{

}

// Hide data in a PNG image
int imc_png_carrier_write(CarrierImage *carrier_img, uint8_t *data, size_t data_len)
{

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
    imc_free(carrier_img->bytes);
    imc_free(carrier_img->carrier);
    imc_free(carrier_img->object);
    __carrier_heap_free(carrier_img);
}

// Close the PNG object and free the memory associated to it
void imc_png_carrier_close(CarrierImage *carrier_img)
{

}

// Free the memory of the data structures used for data hiding
void imc_steg_finish(CarrierImage *carrier_img)
{
    carrier_img->close(carrier_img);
    fclose(carrier_img->file);
    imc_crypto_context_destroy(carrier_img->crypto);
    imc_free(carrier_img);
}