/* Utilities for converting one image format to another. */

#include "imc_includes.h"

// Buffer for storing arbitrary data
typedef struct RawBuffer {
    size_t size;    // Size in bytes of the buffer
    uint8_t *data;  // Array of bytes
} RawBuffer;

// Store the color values and metadata of an image
// This struct should be allocated on the stack and must be initialized to zero.
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

// String to precede error messages related to image conversion
static const char module_name[] = "Image convert";

// Convert an image file to another format
// The converted image is returned as a temporary file, which is automatically
// deleted when it is closed or the program exits.
// In case of failure, NULL is returned and 'imc_codec_error_msg' is set.
FILE *restrict imc_image_convert(FILE *restrict in_file, enum ImageType in_format, enum ImageType out_format)
{
    // Error message in case the conversion fails
    imc_codec_error_msg = NULL;
    
    // Original position on the input file
    fpos_t in_pos;
    int status = fgetpos(in_file, &in_pos);
    if (status != 0)
    {
        file_fail:
        perror(module_name);
        imc_codec_error_msg = "Failed to access input image";
        return NULL;
    }
    status = fseek(in_file, 0, SEEK_SET);
    if (status != 0) goto file_fail;

    // Color values and metadata of the image
    RawImage raw_image = {0};
    bool read_status = false;

    // Read color values and metadata from the input image
    switch (in_format)
    {
        case IMC_JPEG:
            read_status = __read_jpeg(in_file, &raw_image);
            break;
        
        case IMC_PNG:
            read_status = __read_png(in_file, &raw_image);
            break;
        
        case IMC_WEBP:
            read_status = __read_webp(in_file, &raw_image);
            break;
        
        default:
            __close_raw_image(&raw_image);
            if (!imc_codec_error_msg) imc_codec_error_msg = "Invalid input image's format";
            status = fsetpos(in_file, &in_pos);
            return NULL;
            break;
    }

    if (!read_status)
    {
        __close_raw_image(&raw_image);
        if (!imc_codec_error_msg) imc_codec_error_msg = "Failed to read input image";
        status = fsetpos(in_file, &in_pos);
        return NULL;
    }

    // Temporary file for storing the converted image
    FILE *restrict out_file = tmpfile();
    if (!out_file)
    {
        __close_raw_image(&raw_image);
        perror(module_name);
        if (!imc_codec_error_msg) imc_codec_error_msg = "Unable to create temporary file for converting the input image";
        status = fsetpos(in_file, &in_pos);
        return NULL;
    }
    bool write_status = false;
    
    // Write color values and metadata to the output image
    switch (out_format)
    {
        case IMC_JPEG:
            write_status = __write_jpeg(out_file, &raw_image);
            break;
        
        case IMC_PNG:
            write_status = __write_png(out_file, &raw_image);
            break;
        
        case IMC_WEBP:
            write_status = __write_webp(out_file, &raw_image);
            break;
        
        default:
            __close_raw_image(&raw_image);
            fclose(out_file);
            perror(module_name);
            if (!imc_codec_error_msg) imc_codec_error_msg = "Invalid output image's format";
            status = fsetpos(in_file, &in_pos);
            return NULL;
            break;
    }

    if (!write_status)
    {
        __close_raw_image(&raw_image);
        if (!imc_codec_error_msg) imc_codec_error_msg = "Failed to convert input image";
        fclose(out_file);
        status = fsetpos(in_file, &in_pos);
        return NULL;
    }

    // Free the dynamic memory used by the raw image
    __close_raw_image(&raw_image);

    // Reset the original position of the input image
    status = fsetpos(in_file, &in_pos);
    if (status != 0)
    {
        fclose(out_file);
        perror(module_name);
        imc_codec_error_msg = "Failed to access input image";
        return NULL;
    }

    // Return the temporary file of the converted image
    return out_file;
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

// Read the color values and metadata of a JPEG image into a RawImage struct
static bool __read_jpeg(FILE *image_file, struct RawImage *raw_image)
{

}

// Read the color values and metadata of a PNG image into a RawImage struct
static bool __read_png(FILE *image_file, struct RawImage *raw_image)
{
    
}

// Read the color values and metadata of an WebP image into a RawImage struct
static bool __read_webp(FILE *image_file, struct RawImage *raw_image)
{
    WebPDecoderConfig *webp_obj = NULL; // Object used by the WebP image decoder
    uint8_t *file_buffer = NULL;          // Buffer for the raw contents of the WebP file
    size_t file_size = 0;               // Size in bytes of the WebP file

    // Parse the WebP file
    bool status_parse = imc_webp_get_obj(image_file, &webp_obj, &file_buffer, &file_size);
    if (!status_parse)
    {
        return false;
    }

    // Store the image's properties
    raw_image->width = webp_obj->input.width;
    raw_image->height = webp_obj->input.height;
    raw_image->stride = webp_obj->input.width * 4;
    raw_image->has_transparency = webp_obj->input.has_alpha;

    // Allocate enough memory for the uncompressed image
    __alloc_color_buffer(raw_image);

    // Set the decoding parameters
    webp_obj->options.use_threads = 1;          // Multithreaded decoding
    webp_obj->output.colorspace = MODE_RGBA;    // Color channels' order: red, green, blue, alpha
    webp_obj->output.is_external_memory = 1;    // Decode into our buffer rather than one allocated by libweb
    webp_obj->output.u.RGBA.rgba = raw_image->rgba.data;    // Output for the color values
    webp_obj->output.u.RGBA.size = raw_image->rgba.size;    // Size in bytes of the output buffer
    webp_obj->output.u.RGBA.stride = raw_image->stride;     // Size in bytes of a scanline

    // Decode the WebP image
    // Note: the decoded color values are stored at 'raw_image->rgba.data'
    VP8StatusCode status_vp8 = WebPDecode(file_buffer, file_size, webp_obj);
    
    // Clean-up
    free(webp_obj);
    
    // Error handling for the decoding
    if (status_vp8 != VP8_STATUS_OK)
    {
        switch (status_vp8)
        {
            case VP8_STATUS_OUT_OF_MEMORY:
                imc_codec_error_msg = "Not enough memory for decoding the WebP image";
                break;
            
            case VP8_STATUS_NOT_ENOUGH_DATA:
                imc_codec_error_msg = "WebP image is corrupted";
                break;
            
            case VP8_STATUS_UNSUPPORTED_FEATURE:
                imc_codec_error_msg = "WebP image uses an unsupported feature";
                break;
            
            case VP8_STATUS_BITSTREAM_ERROR:
                imc_codec_error_msg = "The file is not a valid WebP image";
                break;
            
            default:
                fprintf(stderr, "Error: unknown issue when decoding the WebP image (%ld).\n", (int64_t)status_vp8);
                imc_codec_error_msg = "This should never happen, please report it as a bug";
                break;
        }
        
        return false;
    }

    /* TO DO: Read the image's metadata */

    // Clean-up
    free(file_buffer);
    
    // Image has been decoded with success
    return true;
}

// Write the color values and metadata of an image into a JPEG file
bool __write_jpeg(FILE *image_file, struct RawImage *raw_image)
{
    
}

// Write the color values and metadata of an image into a PNG file
bool __write_png(FILE *image_file, struct RawImage *raw_image)
{
    
}

// Write the color values and metadata of an image into a WebP file
bool __write_webp(FILE *image_file, struct RawImage *raw_image)
{
    
}

// Free the memory for the color buffer inside a RawImage struct
static void __free_color_buffer(struct RawImage *raw_image)
{
    imc_free(raw_image->rgba.data);
    imc_free(raw_image->row_pointers);
}

// Free all the dynamic memory used by the members of a RawImage struct
static void __close_raw_image(struct RawImage *raw_image)
{
    /* Note: This function is assuming that the pointers are set to NULL if they are unused,
       which is why the RawImage struct must have been initialized to 0 beforehand. */
    __free_color_buffer(raw_image);
    free(raw_image->icc.data);
    free(raw_image->xmp.data);
    free(raw_image->exif.data);
}