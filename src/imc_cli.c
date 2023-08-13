/* Command-line interface for imgconceal */

#include "imc_includes.h"

#define PRINT_ALGORITHM 1001    // Option ID for printing a summary of the algorithm used by this program

// Command line options for imgconceal
static const struct argp_option argp_options[] = {
    {"check", 'c', "IMAGE", 0, "Check if a given JPEG, PNG or WebP image contains data hidden by this program, "\
    "and estimate how much data can still be hidden on the image. "\
    "If a password was used to hide the data, you should also use the '--password' option. ", 1},
    {"extract", 'e', "IMAGE", 0, "Extracts from the cover image the files that were hidden on it by this program. "\
        "The extracted files will have the same names and timestamps as when they were hidden. "\
        "You can also use the '--output' option to specify the folder where the files are extracted into.", 1},
    {"input", 'i', "IMAGE", 0, "Path to the cover image (the JPEG, PNG or WebP file where to hide another file). "\
        "You can also use the '--output' option to specify the name in which to save the modified image.", 2},
    {"output", 'o', "PATH", 0, "When hiding files in an image, this is the filename where "
        "to save the image with hidden data (if this option is not used, the new image is named automatically). "
        "When extracting files from an image, this option is the directory where to save the extracted files "
        "(if not used, the files are extracted to the current working directory).", 2},
    {"hide", 'h', "FILE", 0, "Path to the file being hidden in the cover image. "\
        "This option can be specified multiple times in order to hide more than one file. "\
        "You can also pass more than one path to this option in order to hide multiple files. "\
        "If there is no enough space in the cover image, some files may fail being hidden "\
        "(files specified first have priority when trying to hide). "\
        "The default behavior is to overwrite the existing previously hidden files, "\
        "to avoid that add the '--append' option. "
        "All files are compressed by default, use '--uncompressed' if you want to control that.", 2},
    {"uncompressed", 'u', NULL, 0, "When hiding files, do not compress the files specified with '--hide' after this option. "
    "The files specified before this option get compressed. "
    "If this option is not used, everything gets compressed.", 2},
    {"append", 'a', NULL, 0, "When hiding a file with the '--hide' option, "\
        "append the new file instead of overwriting the existing hidden files. "\
        "For this option to work, the password must be the same as the one used for the previous files.", 3},
    {"password", 'p', "TEXT", 0, "Password for encrypting and scrambling the hidden data. "\
        "This option should be used alongside '--hide', '--extract', or '--check'. "\
        "The password may contain any character that your terminal allows you to input "\
        "(if it has spaces, please enclose the password between quotation marks). "\
        "If you do not want to have a password, please use '--no-password' instead of this option.", 3},
    {"no-password", 'n', NULL, 0, "Do not use a password for encrypting and scrambling the hidden data. "\
        "That means the data will be able to be extracted without needing a password. "
        "This option can be used with '--hide', '--extract', or '--check'." , 4},
    {"verbose", 'v', NULL, 0, "Print detailed progress information.", 5},
    {"silent", 's', NULL, 0, "Do not print any progress information (errors are still shown).", 5},
    {"algorithm", PRINT_ALGORITHM, NULL, 0, "Print a summary of the algorithm used by imgconceal, then exit.", 6},
    {0}
};

// Help text to be shown above the options (when running with '--help')
static const char help_text[] = "\nSteganography tool for hiding and extracting files on JPEG, PNG and WebP images. "\
    "Multiple files can be hidden in a single cover image, "\
    "and the hidden data can be (optionally) protected with a password.\n\n"\
    "Hiding a file on an image:\n"\
    "  imgconceal --input=IMAGE --hide=FILE [--output=NEW_IMAGE] [--append] [--password=TEXT | --no-password]\n\n"\
    "Extracting a hidden file from an image:\n"\
    "  imgconceal --extract=IMAGE [--output=FOLDER] [--password=TEXT | --no-password]\n\n"\
    "Check if an image has data hidden by this program:\n"\
    "  imgconceal --check=IMAGE [--password=TEXT | --no-password]\n\n"\
    "All options:\n";

static const char imgconceal_algorithm_text[] = "The password is hashed using the Argon2id "\
"algorithm, generating a pseudo-random sequence of 64 bytes. The first 32 bytes are used as "\
"the secret key for encrypting the hidden data (XChaCha20-Poly1305 algorithm), while the "\
"last 32 bytes are used to seed the pseudo-random number generator (SHISHUA algorithm) used for "\
"shuffling the positions on the image where the hidden data is written.\n\n"\
\
"In the case of a JPEG cover image, the hidden data is written to the least significant bits of "\
"the quantized AC coefficients that are not 0 or 1 (that happens after the lossy step of the JPEG "\
"algorithm, so the hidden data is not lost). For a PNG or WebP cover image, the hidden data is "\
"written to the least significant bits of the RGB color values of the pixels that are not fully "\
"transparent. Other image formats are not currently supported as cover image, however any file "\
"format can be hidden on the cover image (size permitting). Before encryption, the hidden data is "\
"compressed using the Deflate algorithm.\n\n"\
\
"All in all, the data hiding process goes as:\n"\
"- Hash the password (output: 64 bytes).\n"\
"- Use first half of the hash as the secret key for encryption.\n"\
"- Seed the PRNG with the second half of the hash.\n"\
"- Scan the cover image for suitable bits where hidden data can be stored.\n"\
"- Using the PRNG, shuffle the order in which those bits are going to be written.\n"\
"- Compress the file being hidden.\n"\
"- Encrypt the compressed file.\n"\
"- Break the bytes of the encrypted data into bits.\n"\
"- Write those bits to the cover image (on the shuffled order).\n\n"\
\
"The file's name and timestamps are also stored (both of which are also encrypted), so when "\
"extracted the file has the same name and modified time. The hidden data is extracted by doing "\
"the file operations in reverse order, after hashing the password and unscrambling the read order.\n";

// Options and callback function for the command line interface
static const struct argp argp_struct = {argp_options, &imc_cli_parse_options, NULL, help_text};

// Internal data structure to store the user's options
typedef struct UserOptions {
    char *input;        // Path to the image which will get data hidden into it
    char *output;       // Path where to save the image with hidden data
    char *extract;      // Path to the image with hidden data being extracted
    char *check;        // Path to the image being checked for hidden data
    struct HideList {
        char *data;
        bool uncompressed;
        struct HideList *next;
    } hide;             // Linked list with the paths to the files being hidden on the image
    struct HideList *hide_tail; // Last element of the 'hide' linked list
    PassBuff *password; // Plain text password provided by the user
    int prev_arg;       // The key of the previous parsed command line argument
    bool uncompressed;  // Do not compress the hidden data passed after this flag is set
    bool append;        // Whether the added hidden data is being appended to the existing one
    bool no_password;   // 'true' if not using a password
    bool verbose;       // Prints detailed information during operation
    bool silent;        // Do not print any information during operation
} UserOptions;

// Get a password from the user on the command-line. The typed characters are not displayed.
// They are stored on the 'output' buffer, up to 'buffer_size' bytes.
// Function returns the amount of bytes in the password.
static size_t __get_password(uint8_t *output, const size_t buffer_size)
{
    #ifdef _WIN32   // Windows systems

    // Get the current terminal mode
    DWORD mode = 0;
    HANDLE term = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(term, &mode);

    // Turn off input echoing
    const DWORD old_mode = mode;
    mode &= ~(ENABLE_ECHO_INPUT);
    SetConsoleMode(term, mode);
    
    #else   // Unix systems
    
    // Store the current settings of the terminal
    struct termios old_term, new_term;
    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;

    // Turn off input echoing
    new_term.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_term);
    
    #endif // _WIN32

    size_t pos = 0; // Position on the output buffer
    
    {
        int c = getchar();  // Current character being read
        
        while ( (c != '\r') && (c != '\n') && (pos < buffer_size) )
        {
            output[pos++] = c;
            c = getchar();
        }

        // Discard the remaining characters if the input was truncated
        while ( (c != '\r') && (c != '\n') && (c != EOF) ) {c = getchar();}
    }

    // Turn back to the previous terminal mode
    
    #ifdef _WIN32   // Windows systems
    mode = old_mode;
    SetConsoleMode(term, mode);
    
    #else   // Unix systes
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term);
    #endif  // _WIN32

    return pos;
}

// Allocate memory for a 'PassBuff' struct
static PassBuff *__alloc_passbuff()
{
    PassBuff *pass = sodium_malloc(sizeof(PassBuff));
    if (!pass)
    {
        fprintf(stderr, "Error: No enough memory\n");
        abort();
    }
    sodium_memzero(pass, sizeof(PassBuff));
    
    pass->capacity = sizeof(pass->buffer);
    
    return pass;
}

// Prompt the user to input a password on the terminal.
// The input's maximum size is determined by the macro IMC_PASSWORD_MAX_BYTES,
// being truncated if that size is reached.
// If 'confirm' is true, the user is asked to type the same password again.
// Function returns NULL if the password confirmation failed.
// The returned 'PassBuff' pointer should be freed with 'imc_cli_password_free()'.
static PassBuff *imc_cli_password_input(bool confirm)
{
    PassBuff *pass_1 = __alloc_passbuff();
    
    // Get the password for the first time
    printf("Password: ");
    pass_1->length = __get_password(pass_1->buffer, pass_1->capacity);
    printf("\n");

    if (confirm)
    {
        // Get the password for the second time if corfirmation is enabled
        PassBuff *restrict pass_2 = __alloc_passbuff();
        printf("Repeat password: ");
        pass_2->length = __get_password(pass_2->buffer, pass_2->capacity);
        printf("\n");

        if (
            pass_1->length != pass_2->length ||
            memcmp(pass_1->buffer, pass_2->buffer, pass_1->length) != 0
        )
        {
            // Fail if the two passwords do not match
            sodium_free(pass_1);
            sodium_free(pass_2);
            return NULL;
        }
        
        sodium_free(pass_2);
    }

    // Convert the password to the UTF-8 encoding
    __password_normalize(pass_1, false);

    return pass_1;
}

// Convert an already parsed password string from the system's locale to UTF-8
// 'from_argv' sould be set to 'true' if the string was parsed from the command line options (char *argv[]),
// otherwise its should be 'false' (that is, read from stdin).
static inline void __password_normalize(PassBuff *password, bool from_argv)
{
    // Return if there is no password
    if (!password || password->length == 0) return;
    
    #ifdef _WIN32   // Windows systems

    // Get the terminal's encoding
    const UINT term_cp = from_argv ? CP_ACP : GetConsoleCP();
    /* Note: From my tests, Windows uses the default system's code page when parsing command line arguments,
        but may use a different code page when text is entered by the user on the terminal.
        I believe that this happens because locale is set once the program reaches 'main()' */

    // Convert the terminal input to wide char string
    const int w_pass_len = MultiByteToWideChar(term_cp, 0, password->buffer, password->length, NULL, 0);
    wchar_t w_pass[w_pass_len];
    sodium_mlock(w_pass, sizeof(w_pass));
    MultiByteToWideChar(term_cp, 0, password->buffer, -1, w_pass, w_pass_len);

    // Convert wide char to UTF-8 string
    int u8_pass_len = WideCharToMultiByte(CP_UTF8, 0, w_pass, password->length, NULL, 0, NULL, NULL);;
    char u8_pass[u8_pass_len];
    sodium_mlock(u8_pass, sizeof(u8_pass));
    WideCharToMultiByte(CP_UTF8, 0, w_pass, password->length, u8_pass, u8_pass_len, NULL, NULL);
    sodium_munlock(w_pass, sizeof(w_pass));

    // Clear the old password buffer and copy the new UTF-8 string to it
    sodium_memzero(password->buffer, password->capacity);
    if (u8_pass_len > password->capacity) u8_pass_len = password->capacity;
    memcpy(password->buffer, u8_pass, u8_pass_len);
    sodium_munlock(u8_pass, sizeof(u8_pass));
    password->length = u8_pass_len;

    #else // Linux systems

    if (password->length == 0) return;

    // Open the descriptor for converting text from the system's locale to UTF-8
    iconv_t u8_conv = iconv_open("UTF-8", "");
    if (!u8_conv) return;

    char *const in_text = password->buffer; // Input text in the system's enconding
    size_t in_text_left = password->length; // Amount of bytes of the input remaining to be converted

    const size_t out_size = in_text_left * 4;   // Size of the output buffer (UTF-8 characters can use up to 4 bytes each)
    char out_text[out_size];                    // Buffer for the output text encoded to UTF-8
    sodium_mlock(out_text, sizeof(out_text));
    sodium_memzero(out_text, sizeof(out_text));
    size_t out_text_left = out_size;            // Amount of bytes left in the buffer

    // Pointers to the inut and output buffers
    char *in_text_ptr = in_text;        // Current position on the input
    char *out_text_ptr = &out_text[0];  // Current position on the output
    
    // Convert text to UTF-8
    size_t char_count = iconv(u8_conv, &in_text_ptr, &in_text_left, &out_text_ptr, &out_text_left);
    iconv_close(u8_conv);
    if ( char_count == (size_t)(-1) )
    {
        sodium_munlock(out_text, sizeof(out_text));
        return;
    }

    // Clear the old password buffer and copy the new UTF-8 string to it
    password->length = out_size - out_text_left;
    if (password->length > password->capacity) password->length = password->capacity;
    sodium_memzero(password->buffer, password->capacity);
    memcpy(password->buffer, out_text, password->length);
    sodium_munlock(out_text, sizeof(out_text));

    #endif
}

// Free the memory of a 'PassBuff' struct
static void imc_cli_password_free(PassBuff *password)
{
    sodium_free(password);
    // Note: the above function already overwrites the memory before freeing it.
}

// Get the pointer of the struct needed by the 'argp_parse()' function
const struct argp *restrict imc_cli_get_argp_struct()
{
    return &argp_struct;
}

// Store a copy of the path of a file
static inline void __store_path(const char *path, char **destination)
{
    if (!path) return;
    
    #ifdef _WIN32   // Windows systems
    
    // Convert the terminal input to wide char string
    const int w_path_len = MultiByteToWideChar(CP_ACP, 0, path, -1, NULL, 0);
    wchar_t w_path[w_path_len];
    MultiByteToWideChar(CP_ACP, 0, path, -1, w_path, w_path_len);

    // Convert wide char to UTF-8 string
    const int u8_path_len = WideCharToMultiByte(CP_UTF8, 0, w_path, -1, NULL, 0, NULL, NULL);;
    char *u8_path = imc_malloc(u8_path_len * sizeof(char));
    WideCharToMultiByte(CP_UTF8, 0, w_path, -1, u8_path, u8_path_len, NULL, NULL);

    // Store the unicode path string
    *destination = u8_path;
    
    #else   // Linux systems

    // Duplicate the path string and store it
    *destination = strdup(path);
    
    #endif // _WIN32
}

// Check if an option has not been passed before (program exits if this check fails)
// The idea is to check if the option's value evaluates to 'false'. If it doesn't, then the check fails.
// The error message contains the name of the option, that is why it is needed.
static inline void __check_unique_option(struct argp_state *state, const char *option_name, bool option_value)
{
    if (option_value)
    {
        argp_error(state, "the '%s' option can be used only once.", option_name);
    }
}

// Convert a timespec struct to a date string, and store it on 'out_buff'
static inline void __timespec_to_string(struct timespec *time, char *out_buff, size_t buff_size)
{
    // ISO C 'broken-down time' structure
    struct tm my_time = {0};
    #ifdef _WIN32
    const struct tm *time_status = localtime(&time->tv_sec);    // The 'localtime_r()' function did not work on Windows (with the MSYS2 runtime)
    if (time_status) my_time = *time_status;
    #else
    const struct tm *time_status = localtime_r(&time->tv_sec, &my_time);
    #endif
    
    if (time_status)
    {
        const size_t date_status = strftime(out_buff, buff_size, "%c", &my_time);
        if (!date_status) strncpy(out_buff, "(unknown)", buff_size);
    }
    else
    {
        strncpy(out_buff, "(unknown)", buff_size);
    }
}

// Convert a file size (in bytes) to a string in the appropriate scale, and store it on 'out_buff'
static inline void __filesize_to_string(size_t file_size, char *out_buff, size_t buff_size)
{
    static const char *scale[] = {"bytes", "KB", "MB", "GB", "TB"};
    static const size_t scale_len = sizeof(scale) / sizeof(char *);
    
    // File size as a floating point number
    double size_fp = (double)file_size;
    const char *unit = scale[0];

    // Increase the scale until the value is smaller than 1000 or the maximum scale is reached
    for (size_t i = 0; (i < scale_len - 1) && (size_fp > 1000.0); i++)
    {
        size_fp /= 1000.0;
        unit = scale[i + 1];
    }

    // Store the file size string on the buffer
    if (unit == scale[0])
    {
        // Have no decimals if the unit is 'bytes'
        snprintf(out_buff, buff_size, "%.0f %s", size_fp, unit);
    }
    else
    {
        // Have two decimals for the other units
        snprintf(out_buff, buff_size, "%.2f %s", size_fp, unit);
    }
}

// Validate the command line options, and perform the requested operation
// This is a helper for the 'imc_cli_parse_options()' function.
static inline void __execute_options(struct argp_state *state, void *options)
{
    UserOptions *opt = (UserOptions*)options;

    // Check if the user has specified exactly one operation
    int mode_count = (bool)opt->hide.data + (bool)opt->extract + (bool)opt->check;

    if (mode_count == 0)
    {
        argp_error(state, "you must specify either the 'hide', 'extract', or 'check' option.");
    }
    else if (mode_count != 1)
    {
        argp_error(state, "you can specify only one among the 'hide', 'extract', or 'check' options.");
    }

    // Mode of operation
    enum {HIDE, EXTRACT, CHECK} mode;

    if (opt->hide.data)
    {
        if (opt->input)
        {
            mode = HIDE;
        }
        else
        {
            argp_error(state, "please use '--input' to specify the image where to hide the file.");
        }
    }
    else if (opt->extract)
    {
        mode = EXTRACT;
    }
    else if (opt->check)
    {
        mode = CHECK;
    }
    else
    {
        argp_error(state, "unknown operation.");
    }

    if (mode != HIDE && opt->input)
    {
        argp_error(state, "the 'input' option is used only when hiding a file.");
    }

    if (mode != HIDE && opt->append)
    {
        argp_error(state, "the 'append' option can only be used when hiding a file.");
    }

    if ( (mode != HIDE && mode != EXTRACT) && opt->output )
    {
        argp_error(state, "the 'output' option can only be used when hiding or extracting files.");
    }

    if (mode != HIDE && opt->uncompressed)
    {
        argp_error(state, "the 'uncompressed' option can only be used when hiding files.");
    }

    // Display a password prompt, if a password wasn't provided
    // (and the user did not specify the '--no-password' option)
    if (!opt->password)
    {
        printf("Input password for the hidden file (may be blank)\n");

        if (mode == HIDE)
        {
            opt->password = imc_cli_password_input(true);   // Input the password twice

            if (!opt->password)
            {
                // Exit the program if the user failed to confirm the password
                argp_failure(state, EXIT_FAILURE, 0, "passwords do not match.");
            }
        }
        else // (mode == EXTRACT) || (mode == CHECK)
        {
            opt->password = imc_cli_password_input(false);  // Input the passowrd once
        }
    }

    CarrierImage *steg_image = NULL;    // Info about the image with steganographic data
    char *steg_path = NULL;             // Path to the steganographic image
    int steg_status = 0;                // Return code of the steganographic functions

    // Initialize the image's path
    switch (mode)
    {
        case HIDE:
            steg_path = opt->input;
            break;
        case EXTRACT:
            steg_path = opt->extract;
            break;
        case CHECK:
            steg_path = opt->check;
            break;
    }
    
    // Store the '--verbose' and '--check' flags
    uint64_t flags = 0;
    if (opt->check) flags |= IMC_JUST_CHECK;
    if (opt->verbose && !opt->silent) flags |= IMC_VERBOSE;

    // Initialize the steganography data structure
    // (generate a secret key and seed the pseudo-random number generator)
    steg_status = imc_steg_init(steg_path, opt->password, &steg_image, flags);
    imc_cli_password_free( ((UserOptions*)(state->hook))->password );

    switch (steg_status)
    {
        case IMC_SUCCESS:
            break;
        
        case IMC_ERR_PATH_IS_DIR:
            argp_failure(state, EXIT_FAILURE, 0, "'%s' is a directory; instead of a JPEG, PNG or WebP image.", steg_path);
            break;
        
        case IMC_ERR_FILE_NOT_FOUND:
            argp_failure(state, EXIT_FAILURE, 0, "file '%s' could not be opened. Reason: %s.", steg_path, strerror(errno));
            break;
        
        case IMC_ERR_FILE_INVALID:
            argp_failure(state, EXIT_FAILURE, 0, "file '%s' is not a valid JPEG, PNG or WebP image.", steg_path);
            break;
        
        case IMC_ERR_NO_MEMORY:
            argp_failure(state, EXIT_FAILURE, 0, "no enough memory for hashing the password.");
            break;
        
        default:
            argp_failure(state, EXIT_FAILURE, 0, "unknown error when hashing the password. (%d)", steg_status);
            break;
    }

    // Whether a file has been successfully been hidden on the input image
    bool image_has_changed = false;

    // Operation on the image
    if (mode == HIDE)
    {
        // If on "append mode": Skip to the end of the hidden data
        if (opt->append)
        {
            imc_steg_seek_to_end(steg_image);

            if (steg_image->carrier_pos == 0)
            {
                // Safeguard to prevent the user from overwriting files in case the password is wrong
                argp_failure(state, EXIT_FAILURE, 0,
                    "FAIL: Image '%s' contains no hidden data or the password is incorrect.\n"\
                    "In order to append files to the image, you have to use the same password as the previously files hidden there.\n"\
                    "If you want to overwite the existing hidden files (if any), please run the program without the '--append' option.",
                    basename(steg_path));
            }
        }
        
        // Hide the files on the image
        struct HideList *node = &opt->hide;
        while (node)
        {
            int hide_status = imc_steg_insert(steg_image, node->data, node->uncompressed);

            // Error handling and status messages
            switch (hide_status)
            {
                case IMC_SUCCESS:
                    if (!opt->silent) printf("SUCCESS: hidden '%s' in the cover image.\n", basename(node->data));
                    image_has_changed = true;
                    break;
                
                case IMC_ERR_PATH_IS_DIR:
                    fprintf(stderr, "FAIL: '%s' is a directory, instead of a single file.\n", node->data);
                    break;
                
                case IMC_ERR_FILE_NOT_FOUND:
                    fprintf(stderr, "FAIL: file '%s' could not be opened. Reason: %s\n.", node->data, strerror(errno));
                    break;
                
                case IMC_ERR_NAME_TOO_LONG:
                    fprintf(stderr, "FAIL: file name '%16s...' is too long.\n", basename(node->data));
                    break;
                
                case IMC_ERR_FILE_CORRUPTED:
                    fprintf(stderr, "FAIL: file '%s' is corrupted or might have changed while being hidden.\n", basename(node->data));
                    break;
                
                case IMC_ERR_NO_MEMORY:
                    fprintf(stderr, "FAIL: no enough memory for handling file '%s'.\n", basename(node->data));
                    break;
                
                case IMC_ERR_FILE_TOO_BIG:
                    char size_left[256];
                    __filesize_to_string((steg_image->carrier_lenght - steg_image->carrier_pos) / 8, size_left, sizeof(size_left));
                    fprintf(
                        stderr, "FAIL: no enough space in '%s' to hide '%s' (free space: %s).\n",
                        basename(opt->input), basename(node->data), size_left
                    );
                    break;
                
                case IMC_ERR_CRYPTO_FAIL:
                    fprintf(stderr, "FAIL: could not encrypt '%s'.\n", basename(node->data));
                    break;
                
                default:
                    argp_failure(state, EXIT_FAILURE, 0, "unknown error when hidding data. (%d)", hide_status);
                    break;
            }

            // Move to the next file to be hidden
            node = node->next;
        }
    }
    else // (mode == EXTRACT) || (mode == CHECK)
    {
        bool has_file = false;  // Whether the image contains a hidden file
        
        // Variables used in case the hidden files are being extracted to another folder
        char *cwd_start = NULL;         // The current working directory at the beginning of extraction
        bool outdir_existed = false;    // If the output directory already exists

        // Create the output folder, if one was specified for the extracted files
        if (mode == EXTRACT && opt->output)
        {
            // Remember the current working directory
            #ifdef _WIN32
            cwd_start = _getcwd(NULL, 0);  // Note: this function is allocating memory
            #else // Linux
            cwd_start = getcwd(NULL, 0);   // Note: this function is allocating memory
            #endif

            // Create the output folder
            #ifdef _WIN32
            const int mk_status = _mkdir(opt->output);
            #else // Linux
            const int mk_status = mkdir(opt->output, 0700); // Create with read and write access for only the current user
            #endif

            if (mk_status != 0)
            {
                if (errno == EEXIST)
                {
                    outdir_existed = true;   // We are not deleting the directory in case of error if it already existed
                }
                else
                {
                    // Exit with an error if the directory could not be created and it did not exist
                    argp_failure(
                        state, EXIT_FAILURE, 0,
                        "Could not create output directory '%s'. Reason: %s.\n"
                        "Note: only the last directory of a path is created, its parent directories must exist already.",
                        opt->output, strerror(errno)
                    );
                }
            }

            // Change the current working directory to the output folder
            #ifdef _WIN32
            const int ch_status = _chdir(opt->output);
            #else // Linux
            const int ch_status = chdir(opt->output);
            #endif

            if (ch_status != 0)
            {
                argp_failure(
                    state, EXIT_FAILURE, 0,
                    "Could not extract the hidden files to the directory '%s'. Reason: %s.",
                    opt->output, strerror(errno)
                );
            }
        }
        
        // Save or just check the files hidden on the image
        int unhide_status = IMC_SUCCESS;
        while (unhide_status == IMC_SUCCESS)
        {
            unhide_status = imc_steg_extract(steg_image);
            const char const* image_name = basename(steg_path); // Name of the image with hidden data
            const char const* unhid_name = steg_image->steg_info ? steg_image->steg_info->file_name : ""; // Name of the unhidden file

            // Error handling and status messages
            // Note: after all hidden files have been extracted, the function
            //       will return IMC_ERR_INVALID_MAGIC or IMC_ERR_PAYLOAD_OOB
            switch (unhide_status)
            {
                case IMC_SUCCESS:
                    if (mode == CHECK)
                    {
                        if (has_file) printf("\n");
                        else if (opt->verbose) printf("\n");
                        
                        printf("Found file '%s':\n", steg_image->steg_info->file_name);
                        
                        char str_buffer[256];   // Buffer for the formatted strings

                        __timespec_to_string(&steg_image->steg_info->steg_time, str_buffer, sizeof(str_buffer));
                        printf("  hidden on:     %s\n", str_buffer);

                        __timespec_to_string(&steg_image->steg_info->access_time, str_buffer, sizeof(str_buffer));
                        printf("  last access:   %s\n", str_buffer);

                        __timespec_to_string(&steg_image->steg_info->mod_time, str_buffer, sizeof(str_buffer));
                        printf("  last modified: %s\n", str_buffer);
                        
                        __filesize_to_string(steg_image->steg_info->file_size, str_buffer, sizeof(str_buffer));
                        printf("  size: %s\n", str_buffer);
                    }
                    else // (mode == EXTRACT)
                    {
                        if (!opt->silent)
                        {
                            printf("SUCCESS: extracted '%s' from '%s'.\n", unhid_name, image_name);
                            
                            // The date in which the extracted file was hidden on
                            char date_str[256];
                            __timespec_to_string(&steg_image->steg_info->steg_time, date_str, sizeof(date_str));
                            printf("  hidden on: %s\n", date_str);
                        }
                    }
                    
                    has_file = true;
                    break;
                
                case IMC_ERR_PAYLOAD_OOB:
                    if (!has_file)
                    {
                        fprintf(stderr, "FAIL: image '%s' is too small to contain hidden data.\n", image_name);
                    }
                    break;
                
                case IMC_ERR_INVALID_MAGIC:
                    if (!has_file)
                    {
                        if (mode == CHECK)
                        {
                            char str_buffer[256];
                            __filesize_to_string(steg_image->carrier_lenght / 8, str_buffer, sizeof(str_buffer));
                            printf(
                                "Image '%s' contains no hidden data or the password is incorrect.\n"
                                "This image can hide approximately %s "
                                "(it depends on how well the hidden data can be compressed).\n",
                                image_name,
                                str_buffer
                            );
                        }
                        else // (mode == EXTRACT)
                        {
                            fprintf(stderr, "FAIL: image '%s' contains no hidden data or the password is incorrect.\n", image_name);
                        }
                    }
                    break;
                
                case IMC_ERR_CRYPTO_FAIL:
                    fprintf(stderr, "FAIL: could not decrypt the data on '%s'.\n", image_name);
                    break;
                
                case IMC_ERR_NEWER_VERSION:
                    fprintf(stderr, "FAIL: a newer version of %s was used to hide the data on '%s'.\n", state->name, image_name);
                    break;
                
                case IMC_ERR_FILE_EXISTS:
                    fprintf(stderr, "FAIL: could not save '%s' because a file with the same name already exists.\n", unhid_name);
                    break;
                
                case IMC_ERR_SAVE_FAIL:
                    fprintf(stderr, "FAIL: could not save '%s'. Reason: %s.\n", unhid_name, strerror(errno));
                    break;
                
                default:
                    argp_failure(state, EXIT_FAILURE, 0, "unknown error when extracting hidden data. (%d)", unhide_status);
                    break;
            }
        }

        if (mode == EXTRACT && opt->output)
        {
            // Change the current working directory back to the initial one
            #ifdef _WIN32
            const int ch_status = _chdir(cwd_start);
            #else // Linux
            const int ch_status = chdir(cwd_start);
            #endif

            free(cwd_start);

            // Remove the output directory if no file could be extracted and it didn't exist already
            if (!has_file && !outdir_existed)
            {
                #ifdef _WIN32
                const int ch_status = _rmdir(opt->output);
                #else // Linux
                const int ch_status = rmdir(opt->output);
                #endif
            }
        }

        // Prints how much space the image has left, in case of checking one that already has hidden data
        if (mode == CHECK && has_file)
        {
            char str_buffer[256];
            __filesize_to_string((steg_image->carrier_lenght - steg_image->carrier_pos) / 8, str_buffer, sizeof(str_buffer));
            printf(
                "\nThe cover image '%s' can hide approximately more %s "
                "(after compression of hidden data).\n",
                basename(opt->check),
                str_buffer
            );
        }
    }

    // Save the modified image (when hiding a file)
    if (mode == HIDE && image_has_changed)
    {
        const char *const save_path = opt->output ? opt->output : opt->input;
        const int save_status = imc_steg_save(steg_image, save_path);
        /* Note: The input image will not be overwritten because our file name
           collision resolution is going to append a number to the output's name. */
        
        switch (save_status)
        {
            case IMC_SUCCESS:
                if (!opt->silent)
                {
                    printf("The modified image was saved to '%s'.\n", steg_image->out_path);
                }
                break;
            
            case IMC_ERR_SAVE_FAIL:
                argp_failure(state, EXIT_FAILURE, 0, "file path '%16s...' is too long.", save_path);
                break;
            
            case IMC_ERR_FILE_EXISTS:
                argp_failure(state, EXIT_FAILURE, 0, "could not save '%s' because a file with the same name already exists.", save_path);
                break;
            
            case IMC_ERR_FILE_NOT_FOUND:
                argp_failure(state, EXIT_FAILURE, 0, "could not save '%s'. Reason: %s.", save_path, strerror(errno));
                break;
            
            default:
                argp_failure(state, EXIT_FAILURE, 0, "unknown error when extracting hidden data. (%d)", save_status);
                break;
        }
    }

    // Close the open files and free the memory
    imc_steg_finish(steg_image);
}

// Main callback function for the command line interface
// It receives the user's arguments, then call other parts of the program in order to perform the requested operation.
static int imc_cli_parse_options(int key, char *arg, struct argp_state *state)
{
    if ( (key != ARGP_KEY_INIT) && !state->hook )
    {
        // This should be unreachable if the program is being operated normally
        // This error means that there is either a bug that I didn't notice,
        // or that the user has edited the memory somehow.
        argp_failure(state, EXIT_FAILURE, 0, "memory error.");
    }
    
    // Storage for the user options
    UserOptions *const options = (UserOptions*)(state->hook);
    
    // Handle the Argp's events
    switch (key)
    {
        // Before parsing the options: allocate memory for storing the options
        case ARGP_KEY_INIT:
            state->hook = imc_calloc(1, sizeof(UserOptions));
            break;
        
        // --check: Image to be checked for hidden data
        case 'c':
            __check_unique_option(state, "check", options->check);
            __store_path(arg, &options->check);
            break;
        
        // --extract: Image to have its hidden data extracted
        case 'e':
            __check_unique_option(state, "extract", options->extract);
            __store_path(arg, &options->extract);
            break;
        
        // --input: Image to get data hidden into it
        case 'i':
            __check_unique_option(state, "input", options->input);
            __store_path(arg, &options->input);
            break;
        
        // --output: Where to save the image with hidden data
        case 'o':
            __check_unique_option(state, "output", options->output);
            __store_path(arg, &options->output);
            break;
        
        // --hide: File being hidden on the image
        case 'h':
            hide:
            struct HideList **tail = &options->hide_tail;
            
            // Add the path to the end of the linked list
            if (*tail)
            {
                struct HideList *node = imc_calloc(1, sizeof(struct HideList));
                __store_path(arg, &node->data);
                (*tail)->next = node;
                *tail = node;
            }
            else
            {
                __store_path(arg, &options->hide.data);
                *tail = &options->hide;
            }

            // Set whether the current file is going to remain uncompressed
            (*tail)->uncompressed = options->uncompressed;
            
            break;
        
        // --uncompressed: Do not compress the files added after this flag is set
        case 'u':
            options->uncompressed = true;
            break;
        
        // --append: If the file being hidden is going to be appended to existing ones
        case 'a':
            options->append = true;
            break;
        
        // --password: Password provided by the user
        case 'p':
            if (options->no_password)
            {
                argp_error(state, "you provided a password even though you specified the 'no password' option.");
            }
            
            // Create a password buffer and copy the string to it
            {
                PassBuff *user_password = __alloc_passbuff();
                user_password->length = strlen(arg);
                strncpy(user_password->buffer, arg, IMC_PASSWORD_MAX_BYTES);
                if (user_password->length > IMC_PASSWORD_MAX_BYTES) user_password->length = IMC_PASSWORD_MAX_BYTES;
                
                // Encode the password string to UTF-8
                __password_normalize(user_password, true);
                
                // Store the password
                options->password = user_password;
            }
            
            break;
        
        // --no-password: Do not show a password prompt if the user has not provided a password
        case 'n':
            if (options->password)
            {
                argp_error(state, "you provided a password even though you specified the 'no password' option.");
            }
            options->no_password = true;
            options->password = __alloc_passbuff();   // Store an empty password
            break;
        
        // --verbose: Prints detailed information during operation
        case 'v':
            options->verbose = true;
            break;
        
        // --silent: Do not print detailed information
        case 's':
            options->silent = true;
            break;
        
        // --algorithm: Print the algorithm used by imgconceal, then exit
        case PRINT_ALGORITHM:
            imc_cli_print_algorithm();
            exit(EXIT_SUCCESS);
            break;
        
        // After the last option was parsed: perform the requested operation
        case ARGP_KEY_END:
            if (state->argc <= 1)
            {
                // If no options were passed: display a short help text
                argp_state_help(state, stdout, ARGP_HELP_PRE_DOC | ARGP_HELP_SEE);
                break;
            }

            // Execute the requested operation
            __execute_options(state, state->hook);

            break;
        
        // After the program finished the requested operation: free the options struct
        case ARGP_KEY_FINI:
            free( options->check );
            free( options->extract );
            free( options->input );
            free( options->output );

            // Freeing the linked list
            {
                free( options->hide.data );
                struct HideList *node = options->hide.next;
                while (node)
                {
                    struct HideList *next_node = node->next;
                    imc_free(node->data);
                    imc_free(node);
                    node = next_node;
                };
            }

            imc_free(state->hook);
            state->hook = NULL;
            break;
        
        // When we receive an argument that does not begin with "--" or "-"
        case ARGP_KEY_ARG:
            /* We are going to check if the previous "--" or "-" argument accepts more than one option.
            If so, them we are going to treat the current argument as an option of that "--" or "-" argument. */

            // We are jumping back to the clause that handles the previously parsed argument
            if (options->prev_arg == 'h')
            {
                // The '--hide' argument accepts more than one file to hide
                goto hide;
            }
            
            // Exit with error if an unknown option has been received
            argp_error(state, "unrecognized option '%s'\n"
                "Hint: you should surround an argument with \"quotation marks\" if it contains spaces "
                "or other characters that might confuse the terminal.", arg);
            break;
        
        // Unknown argument
        default:
            return ARGP_ERR_UNKNOWN;
            break;
    }

    // Remember the command line argument that was just parsed (if it begins with "--" or "-")
    if (key != ARGP_KEY_ARG)
    {
        if (state->hook) ((UserOptions*)(state->hook))->prev_arg = key;
    }

    return 0;
}

// Print a summary of imgconceal's algorithm
static void imc_cli_print_algorithm()
{
    printf(imgconceal_algorithm_text);
}

#undef PRINT_ALGORITHM
