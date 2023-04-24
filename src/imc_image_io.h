/* Functions for reading or writing hidden data into a cover image.
 * Supported cover image's formats: JPEG and PNG.
 */

#ifndef _IMC_IMAGE_IO_H
#define _IMC_IMAGE_IO_H

#include "imc_includes.h"

// Flags for the 'imc_steg_init()' function
#define IMC_VERBOSE     (uint64_t)1 // Prints the progress of each step
#define IMC_JUST_CHECK  (uint64_t)2 // Checks for the hidden file's info without saving the file

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
    char *out_path;         // Path where was saved the image with the hidden data
    struct FileMetadata *steg_info; // The metadata of the most recent extracted file
    
    // Manipulation of the file's carrier
    carrier_bytes_t bytes;      // Carrier bytes (same order as on the image)
    carrier_bytes_t *carrier;   // Array of pointers to the carrier bytes of the image (array order is shuffled using the password)
    size_t carrier_lenght;      // Amount of carrier bytes
    size_t carrier_pos;         // Current writting position on the 'carrier' array
    carrier_open_func open;     // Find the carrier bytes
    carrier_save_func save;     // Hide data in the carrier
    carrier_close_func close;   // Free the memory used for the carrier operation
    
    // Operation flags
    bool verbose;       // Whether to print the progress of each operation
    bool just_check;    // Whether to just check for the info of the hidden file instead of saving the file
    
    // Memory management
    void **heap;            // Array of pointers to other heap allocated memory for this image
    size_t heap_lenght;     // Amount of elements on the 'heap' array
} CarrierImage;

// Store the metadata of the hidden file
typedef struct FileMetadata {
    struct timespec access_time;    // Last access time of the file
    struct timespec mod_time;       // Last modified time of the file
    struct timespec steg_time;      // Time when the file was hidden by this program
    size_t file_size;               // Size in bytes of the hidden file
    size_t name_size;               // Size in bytes of the file's name (counting the null terminator)
    char file_name[];               // Name of the file as a C-style string
} FileMetadata;

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

// Internal state of the PNG manipulation functions
typedef struct PngState {
    png_structp object;
    png_infop info;
    png_bytep *row_pointers;
} PngState;

// Initialize an image for hiding data in it
int imc_steg_init(const char *path, const PassBuff *password, CarrierImage **output, uint64_t flags);

// Convenience function for converting the bytes from a timespec struct into
// the byte layout used by this program: 64-bit little endian (each value)
static inline struct timespec64 __timespec_to_64le(struct timespec time);

// Convenience function for converting the bytes from the byte layout used
// by this program (64-bit little endian) to the standard timespec struct
static inline struct timespec __timespec_from_64le(struct timespec64 time);

// Hide a file in an image
// Note: function can be called multiple times in order to hide more files in the same image.
int imc_steg_insert(CarrierImage *carrier_img, const char *file_path);

// Helper function for reading a given amount of bytes (the payload) from the carrier of an image
// Returns 'false' if the read would go out of bounds (no read is done in this case).
// Returns 'true' if the read could be made (the bytes are stored of the provided buffer).
static bool __read_payload(CarrierImage *carrier_img, size_t num_bytes, uint8_t *out_buffer);

// Read the hidden data from the carrier bytes, and save it
// The function extracts and save one file each time it is called.
// So in order to extract all the hidden files, it should be called
// until it stops returning the IMC_SUCCESS status code.
// Note: The filename is stored with the hidden data
int imc_steg_extract(CarrierImage *carrier_img);

// Move the read position of the carrier bytes to right after the end of the last hidden file
// Note: this function is intended to be used when in "append mode" while hiding a file.
void imc_steg_seek_to_end(CarrierImage *carrier_img);

// Progress monitor when reading a JPEG image
static void __jpeg_read_callback(j_common_ptr jpeg_obj);

// Get bytes of a JPEG image that will carry the hidden data
void imc_jpeg_carrier_open(CarrierImage *carrier_img);

// Progress monitor when reading a PNG image
static void __png_read_callback(png_structp png_obj, png_uint_32 row, int pass);

// Get bytes of a PNG image that will carry the hidden data
void imc_png_carrier_open(CarrierImage *carrier_img);

// Change a file path in order to make it unique
// IMPORTANT: Function assumes that the path buffer must be big enough to store the new name.
// (at most 5 characters are added to the path)
static bool __resolve_filename_collision(char *path);

// Copy the "last access" and "last mofified" times from the one file (source) to the other (dest)
static void __copy_file_times(FILE *source_file, const char *dest_path);

// Progress monitor when writing a JPEG image
static void __jpeg_write_callback(j_common_ptr jpeg_obj);

// Write the carrier bytes back to the JPEG image, and save it as a new file
int imc_jpeg_carrier_save(CarrierImage *carrier_img, const char *save_path);

// Progress monitor when writing a PNG image
static void __png_write_callback(png_structp png_obj, png_uint_32 row, int pass);

// Write the carrier bytes back to the PNG image, and save it as a new file
int imc_png_carrier_save(CarrierImage *carrier_img, const char *save_path);

// Free the memory of the array of heap pointers in a CarrierImage struct
static void __carrier_heap_free(CarrierImage *carrier_img);

// Close the JPEG object and free the memory associated to it
void imc_jpeg_carrier_close(CarrierImage *carrier_img);

// Close the PNG object and free the memory associated to it
void imc_png_carrier_close(CarrierImage *carrier_img);

// Save the image with hidden data
int imc_steg_save(CarrierImage *carrier_img, const char *save_path);

// Free the memory of the data structures used for steganography
void imc_steg_finish(CarrierImage *carrier_img);

/* Windows compatibility functions */
#ifdef _WIN32

// Convert a Windows FILETIME struct to a Unix timespec struct
static inline struct timespec __win_filetime_to_timespec(FILETIME win_time);

// From a standard FILE* pointer, get the file handle used by the Windows API
static inline HANDLE __win_get_file_handle(FILE* file_object);

#endif // _WIN32

#endif  // _IMC_IMAGE_IO_H