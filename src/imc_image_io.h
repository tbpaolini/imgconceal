#ifndef _IMC_IMAGE_IO_H
#define _IMC_IMAGE_IO_H

#include "imc_includes.h"

// Carrier: Array with the bytes that carry the hidden data
typedef uint8_t *carrier_bytes_t;

enum ImageType {IMC_JPEG, IMC_PNG};

// Pointers to the steganographic functions
struct CarrierImage;
typedef void (*carrier_open_func)(struct CarrierImage *);
typedef int (*carrier_save_func)(struct CarrierImage *, const char *save_path);
typedef void (*carrier_close_func)(struct CarrierImage *);

// Image that will carry the hidden data
typedef struct CarrierImage
{
    // File parameters
    FILE *file;             // File ponter of the image
    void *object;           // Pointer to the handler that should be passed to the image processing functions
    CryptoContext *crypto;  // Secret parameters generated from the password
    enum ImageType type;    // Format of the image
    
    // Manipulation of the file's carrier
    carrier_bytes_t bytes;      // Carrier bytes (same order as on the image)
    carrier_bytes_t *carrier;   // Array of pointers to the carrier bytes of the image (array order is shuffled using the password)
    size_t carrier_lenght;      // Amount of carrier bytes
    size_t carrier_pos;         // Current writting position on the 'carrier' array
    carrier_open_func open;     // Find the carrier bytes
    carrier_save_func save;     // Hide data in the carrier
    carrier_close_func close;   // Free the memory used for the carrier operation
    
    // Memory management
    void **heap;            // Array of pointers to other heap allocated memory for this image
    size_t heap_lenght;     // Amount of elements on the 'heap' array
} CarrierImage;

// Ensure that the values on our 'timespec struct' will be 64-bit, just to be on the safe side
struct timespec64
{
    int64_t tv_sec;
    int64_t tv_nsec;
};

// Metadata of the file being hidden
// The data is packed in order to avoid discrepancies between compilers,
// since this data will be stored alongside the file.
// IMPORTANT: Values from 'access_time' onwards will be compressed, so they must not be modified after compression.
//            This struct will come before the file, and the compressed/uncompressed sizes also count the file itself.
typedef struct __attribute__ ((__packed__)) FileInfo
{
    // Uncompressed values
    uint32_t version;               // This value should increase whenever this struct changes (for backwards compatibility)
    uint64_t uncompressed_size;     // Size from '.access_time' onwards, after decompressed with Zlib
    uint64_t compressed_size;       // Size from '.access_time' onwards, after compressed with Zlib

    // Compressed values
    struct timespec64 access_time;  // Last access time of the file
    struct timespec64 mod_time;     // Last modified time of the file
    struct timespec64 steg_time;    // Time when the file was hidden by this program
    uint16_t name_size;             // Amount of bytes on the name of the file (counting the null terminator)
    uint8_t file_name[];            // Null-terminated string of the file name (with extension, if any)
} FileInfo;

// Initialize an image for hiding data in it
int imc_steg_init(const char *path, const char *password, CarrierImage **output);

// Convenience function for converting the bytes from a timespec struct into
// the byte layout used by this program: 64-bit little endian (each value)
static inline struct timespec64 __timespec_to_64le(struct timespec time);

// Hide a file in an image
int imc_steg_insert(CarrierImage *carrier_img, const char *file_path);

// Get bytes of a JPEG image that will carry the hidden data
void imc_jpeg_carrier_open(CarrierImage *output);

// Get bytes of a PNG image that will carry the hidden data
void imc_png_carrier_open(CarrierImage *output);

// Change a file path in order to make it unique
// IMPORTANT: Function assumes that the filename has an extension,
// and the path buffer must be big enough to store the new name.
static bool __resolve_filename_collision(char *path);

// Save the carrier bytes back to the JPEG image
int imc_jpeg_carrier_save(CarrierImage *carrier_img, const char *save_path);

// Save the carrier bytes back to the PNG image
int imc_png_carrier_save(CarrierImage *carrier_img, const char *save_path);

// Free the memory of the array of heap pointers in a CarrierImage struct
static void __carrier_heap_free(CarrierImage *carrier_img);

// Close the JPEG object and free the memory associated to it
void imc_jpeg_carrier_close(CarrierImage *carrier_img);

// Close the PNG object and free the memory associated to it
void imc_png_carrier_close(CarrierImage *carrier_img);

// Save the image with hidden data, then free the memory of the data structures used for steganography
void imc_steg_finish(CarrierImage *carrier_img, const char *save_path);

#endif  // _IMC_IMAGE_IO_H