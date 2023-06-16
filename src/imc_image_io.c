/* Functions for reading or writing hidden data into a cover image.
 * Supported cover image's formats: JPEG and PNG.
 */

#include "imc_includes.h"

/* Note: See the 'imc_image_io.h' file for the binary format that we use to store the hidden data. */

static const uint8_t bit[8] = {1, 2, 4, 8, 16, 32, 64, 128};    // Masks for getting each of the 8 bits of a byte
static const uint8_t lsb_get   = 1;     // (0b00000001) Mask for getting the least significant bit of a byte
static const uint8_t lsb_clear = 254;   // (0b11111110) Mask for clearing the least significant bit of a byte

// Info for progress monitoring of PNG images
static _Thread_local double png_num_passes = -1.0;  // How many passes for reading or writing the image
static _Thread_local double png_num_rows = -1.0;    // Image's height
// Note: I am storing these thread local variables, because libpng provides no
//       easy way to access those values from within the row callback function.

// Initialize an image for hiding data in it
int imc_steg_init(const char *path, const PassBuff *password, CarrierImage **output, uint64_t flags)
{
    if (__is_directory(path)) return IMC_ERR_PATH_IS_DIR;
    FILE *image = fopen(path, "rb");
    if (image == NULL) return IMC_ERR_FILE_NOT_FOUND;

    // The file should start with one of these sequences of bytes
    static const uint8_t JPEG_MAGIC[] = {0xFF, 0xD8, 0xFF};
    static const uint8_t PNG_MAGIC[]  = {0x89, 0x50, 0x4E, 0x47};
    static const uint8_t RIFF_MAGIC[] = {'R', 'I', 'F', 'F'};   // First 4 bytes of an WebP image
    static const uint8_t WEBP_MAGIC[] = {'W', 'E', 'B', 'P'};   // Bytes 8 to 11 of an WebP image (counting from 0)

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
    else if (memcmp(img_marker, RIFF_MAGIC, sizeof(RIFF_MAGIC)) == 0)
    {
        // Get the WebP file signature
        // The first 12 bytes should be something like: RIFF....WEBP
        // (where '....' is the file size)
        fseek(image, 8, SEEK_SET);
        fread(img_marker, 1, sizeof(WEBP_MAGIC), image);
        fseek(image, 0, SEEK_SET);

        // Check if the WebP signature matches
        if (memcmp(img_marker, WEBP_MAGIC, sizeof(WEBP_MAGIC)) == 0)
        {
            img_type = IMC_WEBP;
        }
        else
        {
            goto file_magic_error;
        }
    }
    else
    {
        file_magic_error:
        fclose(image);
        return IMC_ERR_FILE_INVALID;
    }

    // Holds the information needed for hiding data in the image
    CarrierImage *carrier_img = imc_calloc(1, sizeof(CarrierImage));
    carrier_img->type = img_type;
    carrier_img->file = image;
    
    // Set up the flags for processing the open image
    if (flags & IMC_JUST_CHECK) carrier_img->just_check = true; // '--check' option
    if (flags & IMC_VERBOSE)    carrier_img->verbose = true;    // '--verbose' option

    // Status message (verbose)
    if (carrier_img->verbose)
    {
        if (password->length > 0) printf("Generating secret key... ");
        else printf("Generating key... ");
    }

    // Generate a secret key, and seed the number generator
    const int crypto_status = imc_crypto_context_create(password, &carrier_img->crypto);
    if (carrier_img->verbose)
    {
        if (crypto_status == IMC_SUCCESS) printf("Done!\n");
        else printf("\n");
    }
    if (crypto_status != IMC_SUCCESS) return crypto_status;

    // Set the struct's methods
    // ("open", "save", and "close" functions for the different supported image formats)
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
        
        case IMC_WEBP:
            carrier_img->open  = &imc_webp_carrier_open;
            carrier_img->save  = &imc_webp_carrier_save;
            carrier_img->close = &imc_webp_carrier_close;
            break;
    }
    
    // Get the carrier bytes from the image
    carrier_img->open(carrier_img);

    // Shuffle the array of pointers
    // (so the order that the bytes are written depends on the password)
    imc_crypto_shuffle_ptr(
        carrier_img->crypto,    // Has the state of the pseudo-random number generator
        (uintptr_t *)(&carrier_img->carrier[0]),    // Beginning of the array
        carrier_img->carrier_lenght,                // Amount of elements on the array
        carrier_img->verbose    // Print the progress if on "verbose" mode
    );
    
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

// Convenience function for converting the bytes from the byte layout used
// by this program (64-bit little endian) to the standard timespec struct
static inline struct timespec __timespec_from_64le(struct timespec64 time)
{
    return (struct timespec){
        .tv_sec  = le64toh(time.tv_sec),
        .tv_nsec = le64toh(time.tv_nsec)
    };
}

// Hide a file in an image
// Note: function can be called multiple times in order to hide more files in the same image.
int imc_steg_insert(CarrierImage *carrier_img, const char *file_path)
{
    if (__is_directory(file_path)) return IMC_ERR_PATH_IS_DIR;
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) return IMC_ERR_FILE_NOT_FOUND;

    // Get the file's metadata

    #ifdef _WIN32   // Windows systems
    
    HANDLE file_handle = __win_get_file_handle(file);   // File handle on Windows
    
    // File size
    LARGE_INTEGER file_size_win = {0};                  // A Windows struct with the file size
    GetFileSizeEx(file_handle, &file_size_win);
    const off_t file_size = file_size_win.QuadPart;     // File size in bytes

    // Timestamps
    FILETIME file_mod_time_win = {0};       // Last modified time (Windows timestamp)
    FILETIME file_access_time_win = {0};    // Last access time (Windows timestamp)
    GetFileTime(file_handle, NULL, &file_access_time_win, &file_mod_time_win);
    const struct timespec file_mod_time = __win_filetime_to_timespec(file_mod_time_win);        // Last modified time (Unix timestamp)
    const struct timespec file_access_time = __win_filetime_to_timespec(file_access_time_win);  // Last access time (Unix timestamp)
    
    #else   // Linux systems
    
    int file_descriptor = fileno(file);
    
    // File size
    struct stat file_stats = {0};
    fstat(file_descriptor, &file_stats);
    const off_t file_size = file_stats.st_size;

    // Timestamps
    const struct timespec file_mod_time = file_stats.st_mtim;       // Last modified time (Unix timestamp)
    const struct timespec file_access_time = file_stats.st_atim;    // Last access time (Unix timestamp)
    
    #endif // _WIN32
    
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
    const char *const file_name = basename(path_temp);
    
    // Calculate the size for the file's metadata that will be stored
    const size_t name_size = strlen(file_name) + 1;
    if (name_size > UINT16_MAX) return IMC_ERR_NAME_TOO_LONG;
    const size_t info_size = sizeof(FileInfo) + name_size;
    
    // Read the file into a buffer
    if (carrier_img->verbose) printf("Loading '%s'... ", file_name);
    const size_t raw_size = info_size + file_size;
    uint8_t *const raw_buffer = imc_malloc(raw_size);
    const size_t read_count = fread(&raw_buffer[info_size], 1, file_size, file);
    fclose(file);
    if (carrier_img->verbose) printf("Done!\n");
    if (read_count != file_size) return IMC_ERR_FILE_CORRUPTED;

    // The offset from which the data will be compressed
    const size_t compressed_offset = offsetof(FileInfo, access_time);
    
    // Store the metadata
    // Note: integers are always stored in little endian byte order.
    FileInfo *file_info = (FileInfo*)raw_buffer;
    
    file_info->version = htole32((uint32_t)IMC_FILEINFO_VERSION);
    file_info->uncompressed_size = htole64(raw_size - compressed_offset);
    file_info->access_time = __timespec_to_64le(file_access_time);
    file_info->mod_time = __timespec_to_64le(file_mod_time);
    file_info->name_size = htole16(name_size);
    
    memcpy(&file_info->file_name[0], file_name, name_size);
    
    // Get the current time (UTC)
    struct timespec current_time = {0};
    #ifdef _WIN32
    timespec_get(&current_time, TIME_UTC);
    #else
    clock_gettime(CLOCK_REALTIME, &current_time);
    #endif
    
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

    #ifdef _WIN32
    uLongf compress_size_win = zlib_buffer_size;
    /* Note: For some reason, on Windows the lenght of the buffer size variable
       is 4 bytes, while on Linux its 8 bytes. Since my code assumes that it is
       going to be 8 bytes, then I am creating this additional variable so I do
       not pass to the compression function a pointer to a size different than
       what the function expects. */
    #endif // _WIN32

    // Compress the data on the buffer (from the '.access_time' onwards)
    if (carrier_img->verbose) printf("Compressing '%s'... ", file_name);
    int zlib_status = compress2(
        &zlib_buffer[compressed_offset],    // Output buffer to store the compressed data (starting after the uncompressed section)
        #ifdef _WIN32
        &compress_size_win,                 // Size in bytes of the output buffer (the function updates the value to the used size)
        #else
        &zlib_buffer_size,                  // Size in bytes of the output buffer (the function updates the value to the used size)
        #endif // _WIN32
        input_buffer,                       // Data being compressed
        file_info->uncompressed_size,       // Size in bytes of the data
        9                                   // Compression level
    );

    #ifdef _WIN32
    zlib_buffer_size = compress_size_win;
    #endif // _WIN32

    if (zlib_status != 0)
    {
        // The only way for decompression to fail here is if no enough memory was available
        imc_clear_free(zlib_buffer, zlib_buffer_size + compressed_offset);
        imc_clear_free(raw_buffer, raw_size);
        if (carrier_img->verbose) printf("\n");
        return IMC_ERR_NO_MEMORY;
    }

    imc_clear_free(raw_buffer, raw_size);
    if (carrier_img->verbose) printf("Done!\n");
    
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
    if (carrier_img->verbose) printf("Encrypting '%s'... ", file_name);
    int crypto_status = imc_crypto_encrypt(
        carrier_img->crypto,    // Has the secret key (generated from the password)
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
        if (carrier_img->verbose) printf("\n");
        return IMC_ERR_CRYPTO_FAIL;
    }

    // Clear and free the buffer of the unencrypted strem
    imc_clear_free(zlib_buffer, zlib_buffer_size);
    if (carrier_img->verbose) printf("Done!\n");

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

        // Status message on verbose (printed once every 512 bytes of data)
        if ( carrier_img->verbose && (i % 512 == 0) )
        {
            const double percent = ((double)i / (double)crypto_size) * 100.0;
            printf_prog("Writing encrypted '%s' to the carrier... %.1f %%\r", file_name, percent);
        }
    }

    if (carrier_img->verbose) printf("Writing encrypted '%s' to the carrier... Done!  \n", file_name);

    // Clear and free the buffer of the encrypted stream
    imc_clear_free(crypto_buffer, crypto_size);

    return IMC_SUCCESS;
}

// Helper function for reading a given amount of bytes (the payload) from the carrier of an image
// Returns 'false' if the read would go out of bounds (no read is done in this case).
// Returns 'true' if the read could be made (the bytes are stored of the provided buffer).
static bool __read_payload(CarrierImage *carrier_img, size_t num_bytes, uint8_t *out_buffer)
{
    if ( (num_bytes * 8) > (carrier_img->carrier_lenght - carrier_img->carrier_pos) )
    {
        // The amount of data left to be read is bigger than the requested amount
        return false;
    }

    memset(out_buffer, 0, num_bytes);

    for (size_t i = 0; i < num_bytes; i++)
    {
        for (size_t j = 0; j < 8; j++)
        {
            // Get the least significant bit from the carrier, then store the bit on the buffer
            const uint8_t carrier_byte = *carrier_img->carrier[carrier_img->carrier_pos++];
            if (carrier_byte & lsb_get) out_buffer[i] |= bit[j];
        }
    }
    
    return true;
}

// Read the hidden data from the carrier bytes, and save it
// The function extracts and save one file each time it is called.
// So in order to extract all the hidden files, it should be called
// until it stops returning the IMC_SUCCESS status code.
// Note: The filename is stored with the hidden data
int imc_steg_extract(CarrierImage *carrier_img)
{
    bool read_status;
    
    // File magic (should be "imcl")
    char magic[IMC_CRYPTO_MAGIC_SIZE];
    memset(magic, 0, sizeof(magic));
    read_status = __read_payload(carrier_img, sizeof(magic)-1, (uint8_t *)magic);
    if (!read_status) return IMC_ERR_PAYLOAD_OOB;

    // Check magic
    if ( strcmp(magic, IMC_CRYPTO_MAGIC) != 0 )
    {
        return IMC_ERR_INVALID_MAGIC;
    }

    // Check the version of the encrypted data
    uint32_t crypto_version;
    read_status = __read_payload(carrier_img, sizeof(crypto_version), (uint8_t *)&crypto_version);
    if (!read_status) return IMC_ERR_PAYLOAD_OOB;
    crypto_version = le32toh(crypto_version);
    if (crypto_version > IMC_CRYPTO_VERSION) return IMC_ERR_NEWER_VERSION;

    // Get the size of the encrypted stream
    uint32_t crypto_size;
    read_status = __read_payload(carrier_img, sizeof(crypto_size), (uint8_t *)&crypto_size);
    if (!read_status) return IMC_ERR_PAYLOAD_OOB;
    crypto_size = le32toh(crypto_size);

    // Get the header from the stream
    uint8_t header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    read_status = __read_payload(carrier_img, sizeof(header), header);
    if (!read_status) return IMC_ERR_PAYLOAD_OOB;
    crypto_size -= sizeof(header);

    // Read the encrypted stream into a buffer
    uint8_t *crypto_buffer = imc_malloc(crypto_size);
    if (carrier_img->verbose && carrier_img->just_check) printf("\n");
    if (carrier_img->verbose) printf("Reading hidden file... ");
    read_status = __read_payload(carrier_img, crypto_size, crypto_buffer);
    if (!read_status)
    {
        imc_free(crypto_buffer);
        if (carrier_img->verbose) printf("\n");
        return IMC_ERR_PAYLOAD_OOB;
    }
    if (carrier_img->verbose) printf("Done!\n");

    // Allocate a buffer for the decrypted data
    unsigned long long decrypt_size = crypto_size - crypto_secretstream_xchacha20poly1305_ABYTES;
    const unsigned long long decrypt_size_start = decrypt_size;
    uint8_t *decrypt_buffer = imc_malloc(decrypt_size);

    // Whether to print a status message for decryption and decompression
    const bool print_msg = carrier_img->verbose && !carrier_img->just_check;

    // Decrypt the data
    if (print_msg) printf("Decrypting hidden file... ");
    int decrypt_status = imc_crypto_decrypt(
        carrier_img->crypto,    // Has the secret key (generated from the password)
        header,                 // Header generated during encryption
        crypto_buffer,          // Encrypted data
        crypto_size,            // Size in bytes of the encrypted data
        decrypt_buffer,         // Output buffer for the decrypted data
        &decrypt_size           // Size in bytes of the output buffer
    );

    if (decrypt_status < 0 || decrypt_size != decrypt_size_start)
    {
        imc_free(crypto_buffer);
        imc_free(decrypt_buffer);
        if (print_msg) printf("\n");
        return IMC_ERR_CRYPTO_FAIL;
    }

    imc_free(crypto_buffer);
    if (print_msg) printf("Done!\n");

    // Current position on the decrypted stream
    size_t d_pos = 0;
    
    // Get the version of the compressed data
    uint32_t compress_version = UINT32_MAX;
    memcpy(&compress_version, &decrypt_buffer[d_pos], sizeof(compress_version));
    compress_version = le32toh(compress_version);
    if (compress_version > IMC_FILEINFO_VERSION)
    {
        imc_free(decrypt_buffer);
        return IMC_ERR_NEWER_VERSION;
    }
    d_pos += sizeof(compress_version);

    // Get the compressed and uncompressed sizes
    uint64_t compress_size, decompress_size;
    
    memcpy(&decompress_size, &decrypt_buffer[d_pos], sizeof(decompress_size));
    decompress_size = le64toh(decompress_size);
    d_pos += sizeof(decompress_size);
    
    memcpy(&compress_size, &decrypt_buffer[d_pos], sizeof(compress_size));
    compress_size = le64toh(compress_size);
    d_pos += sizeof(compress_size);

    // Allocate buffer for decompressed data
    const size_t d_size = d_pos + decompress_size;
    uint8_t *decompress_buffer = imc_malloc(d_size);
    memcpy(&decompress_buffer[0], decrypt_buffer, d_pos);   // Copy the header to the beginning of the buffer

    #ifdef _WIN32
    uLongf decompress_size_win = decompress_size;
    /* Note: For some reason, on Windows the lenght of the buffer size variable
       is 4 bytes, while on Linux its 8 bytes. Since my code assumes that it is
       going to be 8 bytes, then I am creating this additional variable so I do
       not pass to the decompression function a pointer to a size different than
       what the function expects. */
    #endif // _WIN32

    // Decompress the data using Zlib
    if (print_msg) printf("Decompressing hidden file... ");
    int decompress_status = uncompress(
        &decompress_buffer[d_pos],  // Output 
        #ifdef _WIN32
        &decompress_size_win,       // Size of the output buffer
        #else
        &decompress_size,           // Size of the output buffer
        #endif // _WIN32
        &decrypt_buffer[d_pos],     // Input buffer
        compress_size               // Size of the input buffer
    );

    #ifdef _WIN32
    decompress_size = decompress_size_win;
    #endif // _WIN32

    if (decompress_status != 0 || decompress_size + d_pos != d_size)
    {
        // If the file was not tampered with, the actual decompressed size
        // should be exactly the same as the size stored on the metadata
        imc_clear_free(decrypt_buffer, decrypt_size);
        imc_clear_free(decompress_buffer, d_size);
        if (print_msg) printf("\n");
        return IMC_ERR_CRYPTO_FAIL;
    }

    imc_free(decrypt_buffer);
    if (print_msg) printf("Done!\n");
    
    // Get the data needed to reconstruct the hidden file
    FileInfo *file_info = (FileInfo*)decompress_buffer;

    // Calculate the file size
    const size_t name_len = le16toh(file_info->name_size);  // Size of the name's string
    const size_t file_start = sizeof(FileInfo) + name_len;  // Data offset where the file begins
    const size_t file_size  = offsetof(FileInfo, access_time) + decompress_size - file_start;   // Size of the file (bytes)

    // Struct to store the information of the hidden file
    // (since this function can be called multiple times, the struct is only malloc'ed on the first time)
    if (!carrier_img->steg_info)
    {
        carrier_img->steg_info = imc_malloc(sizeof(FileMetadata) + name_len);
    }
    else
    {
        carrier_img->steg_info = imc_realloc(carrier_img->steg_info, sizeof(FileMetadata) + name_len);
    }

    // Store the file's metadata
    *(carrier_img->steg_info) = (FileMetadata){
        .access_time = __timespec_from_64le(file_info->access_time),
        .mod_time = __timespec_from_64le(file_info->mod_time),
        .steg_time = __timespec_from_64le(file_info->steg_time),
        .file_size = file_size,
        .name_size = name_len,
    };

    memcpy( carrier_img->steg_info->file_name, file_info->file_name, name_len );
    
    // If on "check mode": Exit the function without saving the file
    if (carrier_img->just_check)
    {
        imc_free(decompress_buffer);
        return IMC_SUCCESS;
    }

    // Get the last access and last modified times of the hidden file
    struct timespec file_times[2] = {
        __timespec_from_64le(file_info->access_time),
        __timespec_from_64le(file_info->mod_time),
    };

    // Get the name of the hidden file
    char file_name[name_len + 16];  // Extra size added in case it needs to be renamed for avoinding name collision
    memset(file_name, 0, sizeof(file_name));
    memcpy(file_name, file_info->file_name, name_len);

    // On Windows, replace by an underscore the forbidden filename characters
    #ifdef _WIN32
    static const char forbidden_chars[] = "\\/|;:*?<>";
    for (size_t i = 0; i < (name_len - 1); i++)
    {
        char *const my_char = &file_name[i];
        for (size_t j = 0; j < (sizeof(forbidden_chars) - 1); j++)
        {
            if (*my_char == forbidden_chars[j]) *my_char = '_';
        }
        if (iscntrl(*my_char)) *my_char = '_';
    }
    #endif
    /* Note:
        I am doing this because Linux allows some characters that Windows doesn't,
        so the extraction works on Windows, even if the user had a filename on Linux
        that is not allowed on Windows.
        Other than what the operating system itself already disallows, I don't want to
        limit which characters the user can have on filenames. Because my design choice
        is to restore the file as close to the original as possible.
    */
    
    // Make the filename unique (if it already isn't)
    bool is_unique = __resolve_filename_collision(file_name);
    if (!is_unique) return IMC_ERR_FILE_EXISTS;

    // Write the hidden file to disk
    FILE *out_file = fopen(file_name, "wb");
    if (!out_file) return IMC_ERR_SAVE_FAIL;
    if (carrier_img->verbose) printf("Saving extracted file to '%s'... ", file_name);
    fwrite(&decompress_buffer[file_start], file_size, 1, out_file);
    fclose(out_file);
    if (carrier_img->verbose) printf("Done!\n");
    imc_free(decompress_buffer);

    // Restore the file's 'last access' and 'last modified' times
    
    #ifdef _WIN32   // Windows systems
    
    // Convert the file path string to wide char, in order to properly handle UTF-8 characters
    size_t path_len = strlen(file_name) + 1;
    int w_path_len = MultiByteToWideChar(CP_UTF8, 0, file_name, path_len, NULL, 0);
    wchar_t w_path[w_path_len];
    MultiByteToWideChar(CP_UTF8, 0, file_name, path_len, w_path, w_path_len);
    
    // Open the file with only the permission to change its attributes
    HANDLE file_out = CreateFileW(
        w_path,                 // Path to the destination file
        FILE_WRITE_ATTRIBUTES,  // Open file for writing its attributes
        FILE_SHARE_READ,        // Block file's write access to other programs
        NULL,                   // Default security
        OPEN_EXISTING,          // Open the file only if it already exists
        FILE_ATTRIBUTE_NORMAL,  // Normal file (that is, no system or temporary file)
        NULL                    // No template for the attributes
    );
    
    // Write the timestamps to the file's metadata
    if (file_out != INVALID_HANDLE_VALUE)
    {
        FILETIME access_time = __win_timespec_to_filetime(file_times[0]);
        FILETIME mod_time = __win_timespec_to_filetime(file_times[1]);
        SetFileTime(file_out, NULL, &access_time, &mod_time);
        CloseHandle(file_out);
    }
    
    #else   // Unix systems
    
    // Write the timestamps to the file's metadata
    utimensat(AT_FDCWD, file_name, file_times, 0);
    
    #endif // _WIN32

    return IMC_SUCCESS;
}

// Move the read position of the carrier bytes to right after the end of the last hidden file
// Note: this function is intended to be used when in "append mode" while hiding a file.
void imc_steg_seek_to_end(CarrierImage *carrier_img)
{
    // Start from the beginning
    carrier_img->carrier_pos = 0;
    size_t original_pos = 0;
    
    while (true)
    {
        // Read position after the last successfull check
        original_pos = carrier_img->carrier_pos;
        
        // Magic bytes of the current data segment
        char magic[IMC_CRYPTO_MAGIC_SIZE];
        const size_t magic_size = sizeof(magic) - 1;
        memset(magic, 0, sizeof(magic));
        const bool read_success = __read_payload(carrier_img, magic_size, (uint8_t *)(&magic));

        // Keep parsing the data segments the magic bytes are not found
        if ( read_success && (strcmp(magic, IMC_CRYPTO_MAGIC) == 0) )
        {
            // Check the version of the encrypted data
            uint32_t crypto_version = 0;
            {
                const bool read_success = __read_payload(carrier_img, sizeof(crypto_version), (uint8_t *)&crypto_version);
                if (!read_success) break;
            }
            crypto_version = le32toh(crypto_version);
            if (crypto_version > IMC_CRYPTO_VERSION) break;

            // Get the size of the encrypted stream
            uint32_t crypto_size = 0;
            {
                const bool read_success = __read_payload(carrier_img, sizeof(crypto_size), (uint8_t *)&crypto_size);
                if (!read_success) break;
            }
            crypto_size = le32toh(crypto_size);

            // Skip the encrypted stream
            carrier_img->carrier_pos += crypto_size * 8;
        }
        else
        {
            // The magic bytes were not found
            break;
        }
    }

    // Return the read position to where it was before the failed check
    carrier_img->carrier_pos = original_pos;
}

// Progress monitor when reading a JPEG image
static void __jpeg_read_callback(j_common_ptr jpeg_obj)
{
    // Units of work within a reading pass
    const double unit_count = jpeg_obj->progress->pass_counter;
    const double unit_max   = jpeg_obj->progress->pass_limit;

    // Reading passes through the image
    const double pass_count = jpeg_obj->progress->completed_passes;
    const double pass_max   = jpeg_obj->progress->total_passes;

    // Percentage completed
    const double percent = ((pass_count + (unit_count / unit_max)) / pass_max) * 100.0;
    printf_prog("Reading JPEG image... %.1f %%\r", percent);
}

// Get the bytes from a JPEG image that will carry the hidden data
void imc_jpeg_carrier_open(CarrierImage *carrier_img)
{
    // Open the image for reading
    FILE *jpeg_file = carrier_img->file;
    struct jpeg_decompress_struct *jpeg_obj = imc_malloc(sizeof(struct jpeg_decompress_struct));
    struct jpeg_error_mgr *jpeg_err = imc_malloc(sizeof(struct jpeg_error_mgr));
    jpeg_obj->err = jpeg_std_error(jpeg_err);   // Use the default error handler
    jpeg_create_decompress(jpeg_obj);
    jpeg_stdio_src(jpeg_obj, jpeg_file);

    // Save to memory the application markers and comment marker
    // (This is being done in order to preserve the metadata from the original image)
    for (size_t i = 1; i < 16; i++)
    {
        if (i == 14) continue;
        jpeg_save_markers(jpeg_obj, JPEG_APP0+i, 0xFFFF);
        /* Note:
            The JFIF marker (JPEG_APP0) and the Adobe marker (JPEG_APP14) are being skipped
            because libjpeg-turbo already handles those automatically.
        */
    }
    jpeg_save_markers(jpeg_obj, JPEG_COM, 0xFFFF);

    // Setup the progress monitor for the JPEG's read operation
    if (carrier_img->verbose)
    {
        jpeg_obj->progress = imc_calloc(1, sizeof(struct jpeg_progress_mgr));
        jpeg_obj->progress->progress_monitor = &__jpeg_read_callback;
    }

    // Read the DCT coefficients from the image
    jpeg_read_header(jpeg_obj, true);
    jvirt_barray_ptr *jpeg_dct = jpeg_read_coefficients(jpeg_obj);

    // Finish the read's progress monitor
    if (carrier_img->verbose)
    {
        imc_free(jpeg_obj->progress);
        jpeg_obj->progress = NULL;
        printf("Reading JPEG image... Done!  \n");
    }

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

            // Print status message (on verbose)
            if (carrier_img->verbose)
            {
                const double row_count = jpeg_obj->comp_info[comp].height_in_blocks;
                const double row_fraction = ((double)y / row_count) / (double)jpeg_obj->num_components;
                const double comp_fraction = (double)comp / (double)jpeg_obj->num_components;
                const double percent = (comp_fraction + row_fraction) * 100.0;
                printf_prog("Scanning cover image for suitable carrier bits... %.1f %%\r", percent);
            }

            // Iterate column by column from left to right
            for (JDIMENSION x = 0; x < jpeg_obj->comp_info[comp].width_in_blocks; x++)
            {
                // Iterate over the 63 AC coefficients
                // (the DC coefficient of the block is skipped, because modifying it causes a bigger visual impact,
                //  because this coefficient represents the average color of the current block of pixels)
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
                    // (that makes the new image to have nearly the same size as the original image,
                    //  because JPEG compresses zeroes using run length encoding)
                    if (coef != 0 && coef != 1)
                    {
                        // Store the value of the least significant byte of the coefficient
                        carrier_bytes[carrier_count++] = (uint8_t)(coef & (JCOEF)255);
                    }
                }
            }
        }
    }

    // Print status message (on verbose)
    if (carrier_img->verbose)
    {
        printf("Scanning cover image for suitable carrier bits... Done!  \n");
    }

    // Check for edge case
    if (carrier_count == 0)
    {
        fprintf(stderr, "Error: the JPEG image has no suitable bits for hiding the data. "
            "This may happen if the image is just a flat color.\n");
        exit(EXIT_FAILURE);
    }
    
    // Free the unusued space of the array
    carrier_bytes = imc_realloc(carrier_bytes, carrier_count * sizeof(uint8_t));

    // Store the pointers to each element of the bytes array
    carrier_bytes_t *carrier_ptr = imc_calloc(carrier_count, sizeof(uint8_t *));

    for (size_t i = 0; i < carrier_count; i++)
    {
        carrier_ptr[i] = &carrier_bytes[i];
    }

    // Store the output
    carrier_img->bytes = carrier_bytes;             // Array of bytes
    carrier_img->carrier = carrier_ptr;             // Array of pointers to bytes
    carrier_img->carrier_lenght = carrier_count;    // Total amount of pointers to bytes
    carrier_img->object = jpeg_obj;                 // Image handler
    
    // Store the additional heap allocated memory for the purpose of memory management
    carrier_img->heap = imc_malloc(sizeof(void *) * 2);
    carrier_img->heap[0] = (void *)jpeg_err;
    carrier_img->heap[1] = (void *)jpeg_dct;
    carrier_img->heap_lenght = 1;
    /* Note:
        The lenght above is set to 1, even though it is actually 2, because
        the memory of '*jpeg_dct' is managed by libjpeg-turbo (instead of my code).
        The lenght of 1 prevents my code from attempting to free that memory.
    */
}

// Progress monitor when reading a PNG image
static void __png_read_callback(png_structp png_obj, png_uint_32 row, int pass)
{
    const double percent = (((double)pass + ((double)row / png_num_rows)) / png_num_passes) * 100.0;
    printf_prog("Reading PNG image... %.1f %%\r", percent);
}

// Get the bytes from a PNG image that will carry the hidden data
void imc_png_carrier_open(CarrierImage *carrier_img)
{
    // Allocate memory for the PNG processing structs
    png_structp png_obj = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop png_info = png_create_info_struct(png_obj);
    if (!png_obj || !png_info)
    {
        png_destroy_read_struct(&png_obj, &png_info, NULL);
        fprintf(stderr, "Error: No enough memory for reading the PNG file.\n");
        exit(EXIT_FAILURE);
    }

    // Error handling
    if (setjmp(png_jmpbuf(png_obj)))
    {
        png_destroy_read_struct(&png_obj, &png_info, NULL);
        fprintf(stderr, "Error: Failed to read PNG file.\n");
        exit(EXIT_FAILURE);
    }

    // Metadata of the PNG image
    png_uint_32 width;
    png_uint_32 height;
    int bit_depth;
    int color_type;
    int interlace_method;
    int compression_method;
    int filter_method;

    // Parse the metadata from PNG file
    FILE *png_file = carrier_img->file;
    png_init_io(png_obj, png_file);
    png_read_info(png_obj, png_info);
    png_get_IHDR(
        png_obj, png_info,
        &width, &height,
        &bit_depth, &color_type,
        &interlace_method, &compression_method, &filter_method
    );
    
    // If this is a palettized image or the bit depth is smaller than 8,
    // it will be converted so it is non-palettized with bit depth of at least 8.
    // Since we are going to modify the last bits of the image's data, doing so
    // on a palettized image would completely mess the colors because the bytes
    // represent an index on the color palette.
    // And if the bit depth is 1, 2, or 4; changing the last bit would have a
    // noticeable visual impact.
    if ( (color_type & PNG_COLOR_MASK_PALETTE) || (bit_depth < 8) )
    {
        png_set_expand(png_obj);
        png_read_update_info(png_obj, png_info);
        png_get_IHDR(
            png_obj, png_info,
            &width, &height,
            &bit_depth, &color_type,
            &interlace_method, &compression_method, &filter_method
        );
    }

    // Setup the progress monitor (when on verbose)
    if (carrier_img->verbose)
    {
        png_num_passes = (interlace_method == PNG_INTERLACE_ADAM7) ? PNG_INTERLACE_ADAM7_PASSES : 1.0;
        png_num_rows = height;
        png_set_read_status_fn(png_obj, &__png_read_callback);
    }

    // Check if the bit depth has a valid value
    if (bit_depth != 8 && bit_depth != 16)
    {
        png_destroy_read_struct(&png_obj, &png_info, NULL);
        fprintf(stderr, "Error: Failed to read PNG file.\n");
        exit(EXIT_FAILURE);
    }
    /* from this point onwards, this function assumes that the bit depth to be either 8 or 16 */

    // Amount of bytes per row of the image
    const size_t stride = png_get_rowbytes(png_obj, png_info);
    
    // Buffer for storing the image's color values
    const size_t buffer_size = (height * sizeof(png_bytep)) + (height * stride);
    png_bytep *row_pointers = imc_malloc(buffer_size);

    // Pointer to the buffer's position where the values of a row begin
    uintptr_t offset = (uintptr_t)row_pointers + ((size_t)height * sizeof(png_bytep));
    const carrier_bytes_t initial_offset = (carrier_bytes_t)offset;
    
    for (size_t i = 0; i < height; i++)
    {
        // Set the pointers to each row of the image
        row_pointers[i] = (png_bytep)offset;
        offset += stride;
    }
    
    // Read the image into the buffer
    png_read_image(png_obj, row_pointers);
    png_read_end(png_obj, png_info);
    if (carrier_img->verbose) printf("Reading PNG image... Done!  \n");

    const bool has_alpha = color_type & PNG_COLOR_MASK_ALPHA;                   // If the image has transparency
    const png_byte num_channels = png_get_channels(png_obj, png_info);          // Total amount of channels in image
    const png_byte num_colors = has_alpha ? num_channels - 1 : num_channels;    // Amount of channels excluding the alpha channel
    const size_t bytes_per_pixel = num_channels * (bit_depth/8);                // Amount of bytes to represent a single pixel

    // Buffer of pointers to the carrier bytes of the image
    carrier_bytes_t *carrier = imc_malloc(sizeof(carrier_bytes_t) * width * height * num_colors);
    size_t pos = 0;

    // Loop through all pixels in the image to get the carrier bytes
    // (we are going to use pixels with alpha > 0, but the alpha channel itself will not be used as carrier)
    for (size_t y = 0; y < height; y++)
    {
        // Print status message (on verbose)
        if (carrier_img->verbose)
        {
            const double percent = ((double)y / (double)height) * 100.0;
            printf_prog("Scanning cover image for suitable carrier bits... %.1f %%\r", percent);
        }
        
        for (size_t x = 0; x < width; x++)
        {
            uint8_t *const pixel = &row_pointers[y][x * bytes_per_pixel];

            // The bit depths can be either 8 or 16
            // For the later, each color value is stored in big-endian byte order
            if (bit_depth == 8)
            {
                const uint8_t alpha = has_alpha ? pixel[num_channels-1] : UINT8_MAX;
                if (alpha > 0)
                {
                    for (size_t n = 0; n < num_colors; n++)
                    {
                        // Store the pointer to the color value (1 byte)
                        carrier[pos++] = &pixel[n];
                    }
                }
            }
            else    // bit_depth == 16
            {
                // Cast the value to 16-bit unsigned integer, then convert it to the same byte order as the system
                // (16-bit PNG uses the big-endian byte order)
                const uint16_t alpha = has_alpha ? be16toh( *(uint16_t*)(&pixel[(num_channels - 1) * 2]) ) : UINT16_MAX;
                if (alpha > 0)
                {
                    for (size_t n = 0; n < num_colors; n++)
                    {
                        // Store the pointer to the least significant byte of the color value
                        carrier[pos++] = &pixel[1 + (n * 2)];
                    }
                }
            }
        }
    }

    // Print status message (on verbose)
    if (carrier_img->verbose)
    {
        printf("Scanning cover image for suitable carrier bits... Done!  \n");
    }

    // Check for edge case
    if (pos == 0)
    {
        fprintf(stderr, "Error: the PNG image has no suitable bits for hiding the data. "
            "This may happen if the image is fully transparent.\n");
        exit(EXIT_FAILURE);
    }
    
    // Free the unused space of the carrier buffer
    carrier = imc_realloc(carrier, pos * sizeof(carrier_bytes_t));
    
    // Store the structures necessary to handle the opened image
    PngState *state = imc_malloc(sizeof(PngState));
    *state = (PngState){
        .object = png_obj,
        .info = png_info,
        .row_pointers = row_pointers
    };
    carrier_img->object = state;

    // Store the information about the carrier bytes
    carrier_img->carrier = carrier;
    carrier_img->carrier_lenght = pos;
    carrier_img->bytes = initial_offset;
}

// Get the bytes from an WebP image that will carry the hidden data
void imc_webp_carrier_open(CarrierImage *carrier_img)
{
    // Get the total file size of the WebP image

    #ifdef _WIN32   // Windows systems
    
    HANDLE file_handle = __win_get_file_handle(carrier_img->file);
    LARGE_INTEGER file_size_win = {0};
    GetFileSizeEx(file_handle, &file_size_win);
    const size_t file_size = file_size_win.QuadPart;

    #else   // Linux systems
    
    int file_descriptor = fileno(carrier_img->file);
    struct stat file_stats = {0};
    fstat(file_descriptor, &file_stats);
    const size_t file_size = file_stats.st_size;

    #endif

    if (file_size > UINT32_MAX)
    {
        fprintf(stderr, "Error: Maximum size of an WebP image is 4 GB.\n");
        exit(EXIT_FAILURE);
    }

    // Input buffer (original image)
    uint8_t *in_buffer = imc_malloc(file_size);
    const size_t read_count = fread(in_buffer, 1, file_size, carrier_img->file);
    if (read_count != file_size)
    {
        fprintf(stderr, "Error: WebP file could not be read.\n");
        exit(EXIT_FAILURE);
    }

    // Data of the decoded WebP image (original file)
    WebPDecoderConfig *webp_obj = imc_calloc(1, sizeof(WebPDecoderConfig));
    WebPInitDecoderConfig(webp_obj);
    VP8StatusCode status_vp8 = WebPGetFeatures(in_buffer, file_size, &webp_obj->input);

    if (status_vp8 != VP8_STATUS_OK)
    {
        fprintf(stderr, "Error: Could not retrieve the header of the WebP image.\n");
        exit(EXIT_FAILURE);
    }

    if (webp_obj->input.has_animation)
    {
        fprintf(stderr, "Error: Animated WebP images are not supported.\n");
        exit(EXIT_FAILURE);
    }
    
    // Set the decoding options
    webp_obj->options.use_threads = 1;     // Use multithreading
    #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    webp_obj->output.colorspace = MODE_ARGB;    // 32-bit color value on big endian byte order
    #else
    webp_obj->output.colorspace = MODE_BGRA;    // 32-bit color value on little endian byte order
    #endif
    
    // Decode the original image
    status_vp8 = WebPDecode(in_buffer, file_size, webp_obj);
    if (status_vp8 != VP8_STATUS_OK)
    {
        fprintf(stderr, "Error: Could not decode the WebP image. Reason: ");
        switch (status_vp8)
        {
            case VP8_STATUS_OUT_OF_MEMORY:
                fprintf(stderr, "no enough memory.\n");
                break;
            
            case VP8_STATUS_NOT_ENOUGH_DATA:
                fprintf(stderr, "no enough data, the file appears to be corrupted.\n");
                break;
            
            case VP8_STATUS_UNSUPPORTED_FEATURE:
                fprintf(stderr, "image uses an unsupported feature.\n");
                break;
            
            case VP8_STATUS_BITSTREAM_ERROR:
                fprintf(stderr, "not a valid WebP image.\n");
                break;
            
            default:
                fprintf(stderr, "unknown.\n");
                break;
        }
        exit(EXIT_FAILURE);
    }

    /* TO DO: scan for carrier bytes */
}

// Change a file path in order to make it unique
// IMPORTANT: Function assumes that the path buffer must be big enough to store the new name.
// (at most 5 characters are added to the path)
static bool __resolve_filename_collision(char *path)
{
    // Try opening the file for reading to see if it already exists
    FILE *file = fopen(path, "rb");
    if (file == NULL) return true;
    
    // Sanity check (so we don't risk a stack overflow)
    const size_t path_len = strlen(path);
    if (path_len > UINT16_MAX) return false;

    // Get the filename without the directories
    char path_copy[path_len+1];
    memset(path_copy, 0, sizeof(path_copy));
    strncpy(path_copy, path, sizeof(path_copy));
    char *path_base = basename(path_copy);
    
    // Copy the file's extension to a buffer
    char *dot = strrchr(path_base, '.');
    if (!dot) dot = "";
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

// Check if a given path is a directory
static bool __is_directory(const char *path)
{
    #ifdef _WIN32   // Windows systems
    DWORD path_attrib = GetFileAttributes(path);
    if (path_attrib != INVALID_FILE_ATTRIBUTES)
    {
        return (path_attrib & FILE_ATTRIBUTE_DIRECTORY);
    }
    
    #else   // Linux systems
    struct stat path_stat;
    const bool status = stat(path, &path_stat);
    if (status == 0) return S_ISDIR(path_stat.st_mode);
    
    #endif  // _WIN32

    return false;
}

// Copy the "last access" and "last mofified" times from the one file (source) to the other (dest)
static void __copy_file_times(FILE *source_file, const char *dest_path)
{
    #ifdef _WIN32   // Windows systems
    
    // Get the handles of the source and destination files
    // (the destination path string is converted to wide char,
    //  in order to properly handle UTF-8 characters)
    
    HANDLE file_in  = __win_get_file_handle(source_file);
    
    size_t path_len = strlen(dest_path) + 1;
    int w_path_len = MultiByteToWideChar(CP_UTF8, 0, dest_path, path_len, NULL, 0);
    wchar_t w_path[w_path_len];
    MultiByteToWideChar(CP_UTF8, 0, dest_path, path_len, w_path, w_path_len);
    
    HANDLE file_out = CreateFileW(
        w_path,                 // Path to the destination file
        FILE_WRITE_ATTRIBUTES,  // Open file for writing its attributes
        FILE_SHARE_READ,        // Block file's write access to other programs
        NULL,                   // Default security
        OPEN_EXISTING,          // Open the file only if it already exists
        FILE_ATTRIBUTE_NORMAL,  // Normal file (that is, no system or temporary file)
        NULL                    // No template for the attributes
    );
    if (file_out == INVALID_HANDLE_VALUE) return;

    // Timestamps from the source file
    FILETIME access_time = {0}; // Last access time
    FILETIME mod_time = {0};    // Last modified time
    GetFileTime(file_in, NULL, &access_time, &mod_time);

    // Set those timestamps to the destination file
    SetFileTime(file_out, NULL, &access_time, &mod_time);
    CloseHandle(file_out);
    
    #else   // Unix systems
    
    // Get the "last access" and "last mofified" times from the original file
    const int og_descriptor = fileno(source_file);
    struct stat og_stats = {0};
    fstat(og_descriptor, &og_stats);
    const struct timespec og_last_times[2] = {
        og_stats.st_atim,
        og_stats.st_mtim
    };

    // Set those times on the new file to the same as in the original
    utimensat(AT_FDCWD, dest_path, og_last_times, 0);

    #endif // _WIN32
}

// Progress monitor when writing a JPEG image
static void __jpeg_write_callback(j_common_ptr jpeg_obj)
{
    // Units of work within an writing pass
    const double unit_count = jpeg_obj->progress->pass_counter;
    const double unit_max   = jpeg_obj->progress->pass_limit;

    // Writing passes through the image
    const double pass_count = jpeg_obj->progress->completed_passes;
    const double pass_max   = jpeg_obj->progress->total_passes;

    // Percentage completed
    const double percent = ((pass_count + (unit_count / unit_max)) / pass_max) * 100.0;
    printf_prog("Writing JPEG image... %.1f %%\r", percent);
}

// Write the carrier bytes back to the JPEG image, and save it as a new file
int imc_jpeg_carrier_save(CarrierImage *carrier_img, const char *save_path)
{
    // Append the '.jpg' extension to the path, if it does not already end in '.jpg' or '.jpeg'
    const size_t p_len = strlen(save_path);
    if (p_len > UINT16_MAX) return IMC_ERR_SAVE_FAIL;
    char jpeg_path[p_len+16];
    strncpy(jpeg_path, save_path, sizeof(jpeg_path));
    
    if (
        (p_len < 4 || strncmp(&save_path[p_len-4], ".jpg", 4) != 0)
        &&
        (p_len < 5 || strncmp(&save_path[p_len-5], ".jpeg", 5) != 0)
    )
    {
        strcat(jpeg_path, ".jpg");
    }

    // Append a number to the file's stem if the filename already exists
    // Example: 'Image.jpg' might become 'Image (1).jpg'
    // Note: The number goes up to 99, in order to avoid creating too many files accidentally
    bool is_unique = __resolve_filename_collision(jpeg_path);
    if (!is_unique) return IMC_ERR_FILE_EXISTS;

    // Store a copy of the resulting path
    free(carrier_img->out_path);
    carrier_img->out_path = strdup(jpeg_path);

    FILE *jpeg_file = fopen(jpeg_path, "wb");
    if (!jpeg_file) return IMC_ERR_FILE_NOT_FOUND;

    // Create a new JPEG compression object 
    struct jpeg_compress_struct jpeg_obj_out;
    struct jpeg_error_mgr jpeg_err;
    jpeg_obj_out.err = jpeg_std_error(&jpeg_err);   // Use the default error handler
    jpeg_create_compress(&jpeg_obj_out);
    jpeg_stdio_dest(&jpeg_obj_out, jpeg_file);

    // Get the original image
    struct jpeg_decompress_struct *jpeg_obj_in = (struct jpeg_decompress_struct *)carrier_img->object;
    
    // Get the DCT coefficients from the original image
    jvirt_barray_ptr *jpeg_dct = carrier_img->heap[1];
    /* Note:
        The carrier bytes will be stored back to those DCT coefficients.
        Afterwards, the modified coefficients will be saved on the new image.
    */
    
    // Iterate over the color components
    size_t b_pos = 0;
    for (int comp = 0; comp < jpeg_obj_in->num_components; comp++)
    {
        // Iterate row by row from from top to bottom
        for (JDIMENSION y = 0; y < jpeg_obj_in->comp_info[comp].height_in_blocks; y++)
        {
            // Array of DCT coefficients for the current color component
            JBLOCKARRAY coef_array = jpeg_obj_in->mem->access_virt_barray(
                (j_common_ptr)jpeg_obj_in,  // Pointer to the JPEG object
                jpeg_dct[comp],             // DCT coefficients for the current color component
                y,                          // The current row of DCT blocks on the image
                1,                          // Read one row of DCT blocks at a time
                true                        // Opening the array in write mode
            );

            // Print status message (on verbose)
            if (carrier_img->verbose)
            {
                const double row_count = jpeg_obj_in->comp_info[comp].height_in_blocks;
                const double row_fraction = ((double)y / row_count) / (double)jpeg_obj_in->num_components;
                const double comp_fraction = (double)comp / (double)jpeg_obj_in->num_components;
                const double percent = (comp_fraction + row_fraction) * 100.0;
                printf_prog("Writing carrier back to the cover image... %.1f %%\r", percent);
            }

            // Iterate column by column from left to right
            for (JDIMENSION x = 0; x < jpeg_obj_in->comp_info[comp].width_in_blocks; x++)
            {
                // Iterate over the 63 AC coefficients
                // (the DC coefficient of the block is skipped, because modifying it causes a bigger visual impact,
                //  because this coefficient represents the average color of the current block of pixels)
                for (JCOEF i = 1; i < DCTSIZE2; i++)
                {   
                    // The current coefficient
                    const JCOEF coef = coef_array[0][x][i];
                    static const JCOEF coef_lsb = ~(JCOEF)1;    // Mask for clearing the least significant bit

                    // Only the AC coefficients that are not 0 or 1 are used as carriers
                    // (that makes the new image to have nearly the same size as the original image,
                    //  because JPEG compresses zeroes using run length encoding)
                    if (coef != 0 && coef != 1)
                    {
                        // Store the carrier byte
                        coef_array[0][x][i] &= coef_lsb;
                        coef_array[0][x][i] |= carrier_img->bytes[b_pos++];
                    }
                }
            }
        }
    }

    // Print status message (on verbose)
    if (carrier_img->verbose)
    {
        printf("Writing carrier back to the cover image... Done!  \n");
    }

    // Write the modified DCT coefficients into the new image
    jpeg_copy_critical_parameters(jpeg_obj_in, &jpeg_obj_out);
    jpeg_obj_out.optimize_coding = true;
    jpeg_obj_out.write_JFIF_header = jpeg_obj_in->saw_JFIF_marker;
    jpeg_obj_out.write_Adobe_marker = jpeg_obj_in->saw_Adobe_marker;
    jpeg_write_coefficients(&jpeg_obj_out, jpeg_dct);
    /* Note:
        Apparently, libjpeg-turbo does not copy the Huffman tables from the original image to the output:
        https://github.com/libjpeg-turbo/libjpeg-turbo/blob/4e028ecd63aaa13c8a14937f9f1e9a272ed4b543/jctrans.c#L143
        
        It causes the outputs (from different original images) to have the same Huffman tables.
        That would facilitate "fingerprinting" the output, so I am using 'optimize_coding' to generate
        an optimized table for the image, which should make the table be different depending on the image.

        Also by default, the library might create a JFIF or Abobe marker to an image that did not originally had one.
        So I am configuring the encoder to only write those markers if they were seen on the original image.
        It seems that some images have only the EXIF header, instead of also the JFIF one.
        The EXIF data (if present) will be copied on the next step.
    */
    
    // Copy the metadata from the original image into the new image
    jpeg_saved_marker_ptr my_marker = jpeg_obj_in->marker_list;
    while (my_marker)
    {
        jpeg_write_marker(&jpeg_obj_out, my_marker->marker, my_marker->data, my_marker->data_length);
        my_marker = my_marker->next;
    }

    // Setup the progress monitor for the JPEG's write operation
    if (carrier_img->verbose)
    {
        jpeg_obj_out.progress = imc_calloc(1, sizeof(struct jpeg_progress_mgr));
        jpeg_obj_out.progress->progress_monitor = &__jpeg_write_callback;
    }

    // Write the new image to disk
    jpeg_finish_compress(&jpeg_obj_out);
    jpeg_destroy_compress(&jpeg_obj_out);
    fclose(jpeg_file);

    // Finish the write's progress monitor
    if (carrier_img->verbose)
    {
        imc_free(jpeg_obj_out.progress);
        jpeg_obj_out.progress = NULL;
        printf("Writing JPEG image... Done!  \n");
    }

    // Copy the "last access" and "last mofified" times from the original image
    __copy_file_times(carrier_img->file, jpeg_path);

    return IMC_SUCCESS;
}

// Progress monitor when writing a PNG image
static void __png_write_callback(png_structp png_obj, png_uint_32 row, int pass)
{
    const double percent = (((double)pass + ((double)row / png_num_rows)) / png_num_passes) * 100.0;
    printf_prog("Writing PNG image... %.1f %%\r", percent);
}

// Write the carrier bytes back to the PNG image, and save it as a new file
int imc_png_carrier_save(CarrierImage *carrier_img, const char *save_path)
{
    // Append the '.png' extension to the path, if it does not already has the extension
    const size_t p_len = strlen(save_path);
    if (p_len > UINT16_MAX) return IMC_ERR_SAVE_FAIL;
    char png_path[p_len+16];
    strncpy(png_path, save_path, sizeof(png_path));
    
    if ( p_len < 4 || strncmp(&save_path[p_len-4], ".png", 4) != 0 )
    {
        strcat(png_path, ".png");
    }

    // Append a number to the file's stem if the filename already exists
    // Example: 'Image.png' might become 'Image (1).png'
    // Note: The number goes up to 99, in order to avoid creating too many files accidentally
    bool is_unique = __resolve_filename_collision(png_path);
    if (!is_unique) return IMC_ERR_FILE_EXISTS;

    // Store a copy of the resulting path
    free(carrier_img->out_path);
    carrier_img->out_path = strdup(png_path);
    
    // Open the output file for writing
    FILE *png_file = fopen(png_path, "wb");
    if (!png_file) return IMC_ERR_FILE_NOT_FOUND;

    // Retrieve the data from the input PNG file
    PngState *const png_in = (PngState *)carrier_img->object;
    png_structp png_obj_in = png_in->object;
    png_infop png_info_in = png_in->info;
    png_bytep *row_pointers = (png_bytep *)png_in->row_pointers;

    // Create the structures for writing the output PNG image
    png_structp png_obj_out = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop png_info_out  = png_create_info_struct(png_obj_out);
    
    if (!png_obj_out || !png_info_out)
    {
        png_destroy_read_struct(&png_obj_out, &png_info_out, NULL);
        fprintf(stderr, "Error: No enough memory for writing the PNG file.\n");
        exit(EXIT_FAILURE);
    }

    // Error handling
    if (setjmp(png_jmpbuf(png_obj_out)))
    {
        png_destroy_read_struct(&png_obj_out, &png_info_out, NULL);
        fprintf(stderr, "Error: Failed to write PNG file.\n");
        exit(EXIT_FAILURE);
    }
    
    png_init_io(png_obj_out, png_file);

    // Copy the critical parameters from the input
    {
        png_uint_32 width;
        png_uint_32 height;
        int bit_depth;
        int color_type;
        int interlace_method;
        int compression_method;
        int filter_method;

        png_get_IHDR(
            png_obj_in, png_info_in,
            &width, &height,
            &bit_depth, &color_type,
            &interlace_method, &compression_method, &filter_method
        );

        png_set_IHDR(
            png_obj_out, png_info_out,
            width, height,
            bit_depth, color_type,
            interlace_method, compression_method, filter_method
        );
    }

    // Copy the text comments from the input
    // (this also includes the XMP metadata)
    {
        png_textp text;
        int num_text = 0;
        png_get_text(png_obj_in, png_info_in, &text, &num_text);
        if (num_text > 0)
        {
            png_set_text(png_obj_out, png_info_out, text, num_text);
        }
    }

    // Copy the EXIF metadata from the input
    {
        png_bytep exif;
        png_uint_32 num_exif = 0;
        png_get_eXIf_1(png_obj_in, png_info_in, &num_exif, &exif);
        if (num_exif > 0)
        {
            png_set_eXIf_1(png_obj_out, png_info_out, num_exif, exif);
        }
    }

    // Copy the gamma value from the input
    {
        double gamma;
        png_uint_32 status = png_get_gAMA(png_obj_in, png_info_in, &gamma);
        if (status == PNG_INFO_gAMA)
        {
            png_set_gAMA(png_obj_out, png_info_out, gamma);
        }
    }

    // Copy the primary chromaticities from the input
    {
        double white_x, white_y, red_x, red_y, green_x, green_y, blue_x, blue_y;
        png_uint_32 status = png_get_cHRM(
            png_obj_in, png_info_in,
            &white_x, &white_y,
            &red_x, &red_y,
            &green_x, &green_y,
            &blue_x, &blue_y
        );

        if (status == PNG_INFO_cHRM)
        {
            png_set_cHRM(
                png_obj_out, png_info_out,
                white_x, white_y,
                red_x, red_y,
                green_x, green_y,
                blue_x, blue_y
            );
        }
    }

    // Copy the stardand RGB color space from the input
    {
        int srgb_intent;
        png_uint_32 status = png_get_sRGB(png_obj_in, png_info_in, &srgb_intent);
        if (status == PNG_INFO_sRGB)
        {
            png_set_sRGB(png_obj_out, png_info_out, srgb_intent);
        }
    }
    
    // Copy the color profile from the input
    {
        png_charp name;
        int compression_type;
        png_bytep profile;
        png_uint_32 proflen;
        png_uint_32 status = png_get_iCCP(png_obj_in, png_info_in, &name, &compression_type, &profile, &proflen);
        if (status == PNG_INFO_iCCP)
        {
            png_set_iCCP(png_obj_out, png_info_out, name, compression_type, profile, proflen);
        }
    }

    // Copy the background color from the input
    {
        png_color_16p background;
        png_uint_32 status = png_get_bKGD(png_obj_in, png_info_in, &background);
        if (status == PNG_INFO_bKGD)
        {
            png_set_bKGD(png_obj_out, png_info_out, background);
        }
    }

    // Copy the screen offsets from the input
    {
        png_int_32 offset_x, offset_y;
        int unit_type;
        png_uint_32 status = png_get_oFFs(png_obj_in, png_info_in, &offset_x, &offset_y, &unit_type);
        if (status == PNG_INFO_oFFs)
        {
            png_set_oFFs(png_obj_out, png_info_out, offset_x, offset_y, unit_type);
        }
    }

    // Copy the physical dimensions from the input
    {
        png_uint_32 res_x, res_y;
        int unit_type;
        png_uint_32 status = png_get_pHYs(png_obj_in, png_info_in, &res_x, &res_y, &unit_type);
        if (status == PNG_INFO_pHYs)
        {
            png_set_pHYs(png_obj_out, png_info_out, res_x, res_y, unit_type);
        }
    }

    // Copy the significant bits from the input
    {
        png_color_8p sig_bit;
        png_uint_32 status = png_get_sBIT(png_obj_in, png_info_in, &sig_bit);
        if (status == PNG_INFO_sBIT)
        {
            png_set_sBIT(png_obj_out, png_info_out, sig_bit);
        }
    }

    // Copy the modified time from the input
    {
        png_timep mod_time;
        png_uint_32 status = png_get_tIME(png_obj_in, png_info_in, &mod_time);
        if (status == PNG_INFO_tIME)
        {
            png_set_tIME(png_obj_out, png_info_out, mod_time);
        }
    }

    // Copy the pixel calibration from the input
    {
        png_charp purpose;
        png_int_32 X0;
        png_int_32 X1;
        int type;
        int nparams;
        png_charp units;
        png_charpp params;
        
        png_uint_32 status = png_get_pCAL(
            png_obj_in, png_info_in,
            &purpose, &X0, &X1, &type,
            &nparams, &units, &params
        );

        if (status == PNG_INFO_pCAL)
        {
            png_set_pCAL(
                png_obj_out, png_info_out,
                purpose, X0, X1, type,
                nparams, units, params
            );
        }
    }

    // Copy the physical scale from the input
    {
        int unit;
        double width, height;
        png_uint_32 status = png_get_sCAL(png_obj_in, png_info_in, &unit, &width, &height);
        if (status == PNG_INFO_sCAL)
        {
            png_set_sCAL(png_obj_out, png_info_out, unit, width, height);
        }
    }

    // Setup the progress monitor (when on verbose)
    if (carrier_img->verbose)
    {
        png_set_write_status_fn(png_obj_out, &__png_write_callback);
    }

    // Write the copied data to the output image
    png_write_info(png_obj_out, png_info_out);

    // Write the color values to the output image
    png_write_image(png_obj_out, row_pointers);

    // Finish saving the output image
    png_write_end(png_obj_out, png_info_out);
    png_destroy_write_struct(&png_obj_out, &png_info_out);
    fclose(png_file);
    if (carrier_img->verbose) printf("Writing PNG image... Done!  \n");

    // Copy the "last access" and "last mofified" times from the original image
    __copy_file_times(carrier_img->file, png_path);

    return IMC_SUCCESS;
}

// Progress monitor when writing a PNG image
static int __webp_write_callback(int percent, const WebPPicture* webp_obj)
{

}

// Write the carrier bytes back to the WebP image, and save it as a new file
int imc_webp_carrier_save(CarrierImage *carrier_img, const char *save_path)
{

}

// Free the memory of the array of heap pointers in a CarrierImage struct
static void __carrier_heap_free(CarrierImage *carrier_img)
{
    for (size_t i = 0; i < carrier_img->heap_lenght; i++)
    {
        imc_free(carrier_img->heap[i]);
    }
    imc_free(carrier_img->heap);
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
    PngState *const png = (PngState *)carrier_img->object;
    png_destroy_read_struct(&png->object, &png->info, NULL);
    imc_free(png->row_pointers);
    imc_free(carrier_img->carrier);
    __carrier_heap_free(carrier_img);
    free(png);
}

// Close the WebP object and free the memory associated to it
void imc_webp_carrier_close(CarrierImage *carrier_img)
{
    
}

// Save the image with hidden data
int imc_steg_save(CarrierImage *carrier_img, const char *save_path)
{
    return carrier_img->save(carrier_img, save_path);
}

// Free the memory of the data structures used for steganography
void imc_steg_finish(CarrierImage *carrier_img)
{
    // Close the open files
    carrier_img->close(carrier_img);
    fclose(carrier_img->file);

    // Free the memory used by the steganographic operations
    imc_crypto_context_destroy(carrier_img->crypto);
    imc_free(carrier_img->out_path);
    imc_free(carrier_img->steg_info);
    imc_free(carrier_img);
}

// Print text at most once each 1/6 second
// Note: function intended for the progress monitor, it uses the same format as 'printf()'.
void printf_prog(const char *format, ...)
{
    static const clock_t wait_millis = 166;   // Amount of milliseconds to wait before printing again
    static _Thread_local clock_t last_time = -wait_millis;  // Timestamp (in milliseconds) when printed for the last time
    
    // Get the current timestamp (in milliseconds)
    clock_t now = (clock() * 1000) / CLOCKS_PER_SEC;
    
    // Print the formatted text if at least 166 milliseconds have passed
    if (now - last_time >= wait_millis)
    {
        va_list arguments;
        va_start(arguments, format);
        vprintf(format, arguments);
        fflush(stdout);
        va_end(arguments);
        last_time = now;
    }
}

/* Windows compatibility functions */
#ifdef _WIN32

// Convert a Windows FILETIME struct to a Unix timespec struct
static inline struct timespec __win_filetime_to_timespec(FILETIME win_time)
{
    // Unix timestamp
    ULARGE_INTEGER unix_time;
    unix_time.LowPart = win_time.dwLowDateTime;
    unix_time.HighPart = win_time.dwHighDateTime;

    // Convert the FILETIME to the number of 100-nanosecond intervals since January 1st 1970 (the Unix epoch)
    // Note: the Windows epoch is on January 1st 1601.
    unix_time.QuadPart -= 116444736000000000ULL;

    // Convert the number of 100-nanosecond intervals to microseconds
    unix_time.QuadPart /= 10;

    // Set the timespec fields
    struct timespec output = {
        .tv_sec = unix_time.QuadPart / 1000000,             // Seconds since the Unix epoch
        .tv_nsec = (unix_time.QuadPart % 1000000) * 1000,   // Nanoseconds on the current second
    };
    
    return output;
}

// Convert a Unix timespec struct to a Windows FILETIME struct
static inline FILETIME __win_timespec_to_filetime(struct timespec unix_time)
{
    // Windows timestamp (number of 100-nanosecond intervals since January 1st 1601)
    // Note: The Unix timestamo is the number of seconds since January 1st 1970
    ULARGE_INTEGER win_time;
    win_time.QuadPart = (unix_time.tv_sec * 10000000ULL) + (unix_time.tv_nsec / 100ULL) + 116444736000000000ULL;
    
    // Windows file's time
    FILETIME output;
    output.dwLowDateTime = win_time.LowPart;
    output.dwHighDateTime = win_time.HighPart;
    
    return output;
}

// From a standard FILE* pointer, get the file handle used by the Windows API
static inline HANDLE __win_get_file_handle(FILE* file_object)
{
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
    return (HANDLE)_get_osfhandle(fileno(file_object));
    #pragma GCC diagnostic pop
}

// Return a pointer to the file name on a path (without the leading directories or slashes)
// Note: This is a rewrite of the POSIX function of same name, which is not present on Windows.
char *basename(char *path)
{
    // Search for the last backslash
    char *file_name = strrchr(path, '\\');
    if (file_name) return (file_name + 1);
    
    // Search for the last slash
    file_name = strrchr(path, '/');
    if (file_name) return (file_name + 1);

    // If neither was found, just return the name as is
    return path;
}

#endif // _WIN32