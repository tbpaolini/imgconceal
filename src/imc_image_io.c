#include "imc_includes.h"

static const uint8_t bit[8] = {1, 2, 4, 8, 16, 32, 64, 128};    // Masks for getting each of the 8 bits of a byte
static const uint8_t lsb_get   = 0b00000001;    // Mask for clearing the least significant bit of a byte
static const uint8_t lsb_clear = 0b11111110;    // Mask for clearing the least significant bit of a byte

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
            carrier_img->save  = &imc_jpeg_carrier_save;
            carrier_img->close = &imc_jpeg_carrier_close;
            break;
        
        case IMC_PNG:
            carrier_img->open  = &imc_png_carrier_open;
            carrier_img->save  = &imc_png_carrier_save;
            carrier_img->close = &imc_png_carrier_close;
            break;
    }
    
    // Get the carrier bytes from the image
    carrier_img->open(carrier_img);
    
    *output = carrier_img;
    return IMC_SUCCESS;
}

// Convenience function for converting the bytes from a timespec struct into
// the byte layout used by this program: 64-bit little endian (each value)
static inline struct timespec64 __timespec_to_64le(struct timespec time)
{
    return (struct timespec64){
        .tv_sec  = htole64((int64_t)time.tv_sec),
        .tv_nsec = htole64((int64_t)time.tv_nsec)
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
    const size_t raw_size = info_size + file_size;
    uint8_t *const raw_buffer = imc_malloc(raw_size);
    const size_t read_count = fread(&raw_buffer[info_size], 1, file_size, file);
    fclose(file);
    if (read_count != file_size) return IMC_ERR_FILE_INVALID;

    // The offset from which the data will be compressed
    const size_t compressed_offset = offsetof(FileInfo, access_time);
    
    // Store the metadata
    // Note: integers are always stored in little endian byte order.
    FileInfo *file_info = (FileInfo*)raw_buffer;
    
    file_info->version = htole32((uint32_t)IMC_FILEINFO_VERSION);
    file_info->uncompressed_size = htole64(raw_size - compressed_offset);
    file_info->access_time = __timespec_to_64le(file_stats.st_atim);
    file_info->mod_time = __timespec_to_64le(file_stats.st_mtim);
    file_info->name_size = htole16(name_size);
    
    memcpy(&file_info->file_name[0], file_name, name_size);
    struct timespec current_time;
    clock_gettime(CLOCK_REALTIME, &current_time);
    
    file_info->steg_time = __timespec_to_64le(current_time);

    // Create a buffer for the compressed data
    // Note: For the overhead calculation, see https://zlib.net/zlib_tech.html
    const size_t zlib_overhead = 6 + (5 * (file_size / 16000)) + 1;
    size_t zlib_buffer_size = raw_size + zlib_overhead;
    uint8_t *const input_buffer = (uint8_t *)(&file_info->access_time);
    uint8_t *zlib_buffer = imc_malloc(zlib_buffer_size);
    
    // Copy the uncompressed metadata to the beginning of the buffer
    memcpy(zlib_buffer, file_info, compressed_offset);
    zlib_buffer_size -= compressed_offset;
    
    // Compress the data on the buffer (from the '.access_time' onwards)
    int zlib_status = compress2(
        &zlib_buffer[compressed_offset],    // Output buffer to store the compressed data (starting after the uncompressed section)
        &zlib_buffer_size,                  // Size in bytes of the output buffer (the function updates the value to the used size)
        input_buffer,                       // Data being compressed
        file_info->uncompressed_size,       // Size in bytes of the data
        9                                   // Compression level
    );

    if (zlib_status != 0)
    {
        // The only way for decompression to fail here is if no enough memory was available
        imc_clear_free(zlib_buffer, zlib_buffer_size + compressed_offset);
        imc_clear_free(raw_buffer, raw_size);
        return IMC_ERR_NO_MEMORY;
    }

    imc_clear_free(raw_buffer, raw_size);
    
    // Store the actual size of the compressed data
    ((FileInfo *)zlib_buffer)->compressed_size = htole64(zlib_buffer_size);
    zlib_buffer_size += compressed_offset;

    // Free the unused space in the output buffer
    zlib_buffer = imc_realloc(zlib_buffer, zlib_buffer_size);

    // Total size of the encrypted stream
    const size_t crypto_size = IMC_CRYPTO_OVERHEAD + zlib_buffer_size;

    if (crypto_size * 8 > carrier_img->carrier_lenght - carrier_img->carrier_pos)
    {
        // The carrier is not big enough to store the encrypted stream
        imc_clear_free(zlib_buffer, zlib_buffer_size);
        return IMC_ERR_FILE_TOO_BIG;
    }
    
    // Allocate the buffer for the encrypted stream
    uint8_t *const crypto_buffer = imc_malloc(crypto_size);
    unsigned long long crypto_output_len;
    
    // Encrypt the data stream
    int crypto_status = imc_crypto_encrypt(
        carrier_img->crypto,    // Secret key (generated from the password)
        zlib_buffer,            // Unencrypted data stream
        zlib_buffer_size,       // Size in bytes of the unencrypted stream
        crypto_buffer,          // Output buffer for the encrypted data
        &crypto_output_len      // Stores the amount of bytes written to the output buffer
    );

    if (crypto_status < 0)
    {
        // It does not seem that encryption can fail, if the parameters are correct and the buffer is big enough.
        // But I still am doing this check here, just to be on the safe side.
        imc_clear_free(zlib_buffer, zlib_buffer_size);
        imc_clear_free(crypto_buffer, crypto_size);
        return IMC_ERR_CRYPTO_FAIL;
    }

    // Clear and free the buffer of the unencrypted strem
    imc_clear_free(zlib_buffer, zlib_buffer_size);

    // Store the encrypted data stream on the least significant bits of the carrier
    for (size_t i = 0; i < crypto_size; i++)
    {
        for (size_t j = 0; j < 8; j++)
        {
            // Get a pointer to the carrier byte
            uint8_t *const carrier_byte = carrier_img->carrier[carrier_img->carrier_pos++];
            
            // Get the data bit to be hidden on the carrier
            const uint8_t my_bit = (crypto_buffer[i] & bit[j]) != 0;
            
            // Clear the least significant bit of the carrier, then store the data bit there
            *carrier_byte &= lsb_clear;
            *carrier_byte |= my_bit;
        }
    }

    // Clear and free the buffer of the unencrypted strem
    imc_clear_free(crypto_buffer, crypto_size);

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

                    // Only the AC coefficients that are not 0 or 1 are used as carriers
                    if (coef != 0 && coef != 1)
                    {
                        // Store the value of the least significant byte of the coefficient
                        carrier_bytes[carrier_count++] = (uint8_t)(coef & (JCOEF)255);
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

// Change a file path in order to make it unique
// IMPORTANT: Function assumes that the filename has an extension,
// and the path buffer must be big enough to store the new name.
static bool __resolve_filename_collision(char *path)
{
    // Try opening the file for reading to see if it already exists
    FILE *file = fopen(path, "rb");
    if (file == NULL) return true;
    
    // Copy the file's extension to a buffer
    char *dot = strrchr(path, '.');
    const size_t e_len = strlen(dot);
    char extension[e_len+1];
    memset(extension, 0, sizeof(extension));
    strncpy(extension, dot, sizeof(extension));

    // Copy the file's stem to a buffer
    const size_t s_len = strlen(path) - e_len;
    char stem[s_len+1];
    memset(stem, 0, sizeof(stem));
    strncpy(stem, path, s_len);
    
    for (int i = 1; i <= IMC_MAX_FILENAME_DUPLICATES; i++)
    {
        fclose(file);

        // Create a 'number of the copy' string
        char copy_num[6];
        snprintf(copy_num, sizeof(copy_num), " (%d)", i);

        // Concatenate the stem, number, and extension to form a new filename
        memcpy(path, stem, sizeof(stem));
        strcat(path, copy_num);
        strcat(path, extension);

        // Test if the new filename exists
        file = fopen(path, "rb");
        if (file == NULL) return true;
    }

    // No new name could be created
    // (the amount of tries is limited to 99)
    fclose(file);
    return false;
}

// Save the carrier bytes back to the JPEG image
int imc_jpeg_carrier_save(CarrierImage *carrier_img, const char *save_path)
{
    // Append the '.jpg' extension to the path, if it does not already end in '.jpg' or '.jpeg'
    const size_t p_len = strlen(save_path);
    char jpeg_path[p_len+16];
    strncpy(jpeg_path, save_path, sizeof(jpeg_path));
    
    if ( (strncmp(&save_path[p_len-4], ".jpg", 4) != 0) && (strncmp(&save_path[p_len-5], ".jpeg", 5) != 0) )
    {
        strcat(jpeg_path, ".jpg");
    }

    // Append a number to the file's stem if the filename already exists
    // Example: 'Image.jpg' might become 'Image (1).jpg'
    // Note: The number goes up to 99, in order to avoid creating too many files accidentally
    bool is_unique = __resolve_filename_collision(jpeg_path);
    if (!is_unique) return IMC_ERR_FILE_EXISTS;
    
    // Create a new JPEG compression object 
    FILE *jpeg_file = fopen(jpeg_path, "wb");
    struct jpeg_compress_struct jpeg_obj;
    struct jpeg_error_mgr jpeg_err;
    jpeg_obj.err = jpeg_std_error(&jpeg_err);   // Use the default error handler
    jpeg_create_compress(&jpeg_obj);
    
    jpeg_stdio_dest(&jpeg_obj, jpeg_file);
}

// Save the carrier bytes back to the PNG image
int imc_png_carrier_save(CarrierImage *carrier_img, const char *save_path)
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

// Save the image with hidden data, then free the memory of the data structures used for steganography
void imc_steg_finish(CarrierImage *carrier_img, const char *save_path)
{
    carrier_img->save(carrier_img, save_path);
    carrier_img->close(carrier_img);
    fclose(carrier_img->file);
    imc_crypto_context_destroy(carrier_img->crypto);
    imc_free(carrier_img);
}