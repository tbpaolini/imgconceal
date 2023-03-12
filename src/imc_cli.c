/* Command-line interface for imgconceal */

#include "imc_includes.h"

// Command line options for imgconceal
static const struct argp_option argp_options[] = {
    {"check", 'c', "IMAGE", 0, "Check if a given JPEG or PNG image contains data hidden by this program, "\
    "and estimate how much data can still be hidden on the image. "\
    "If a password was used to hide the data, you should also use the '--password' option.", 1},
    {"extract", 'e', "IMAGE", 0, "Extracts from the cover image the files that were hidden on it by this program."\
        "The extracted files will have the same names and timestamps as when they were hidden.", 1},
    {"input", 'i', "IMAGE", 0, "Path to the cover image (the JPEG or PNG file where to hide another file). "\
        "Please use the '--output' option to specify where to save the modified image.", 2},
    {"output", 'o', "IMAGE", 0, "Path to where to save the image with hidden data. "\
        "If this option is not used, the output file will be named automatically "\
        "(a number is added to the name of the original file).", 2},
    {"hide", 'h', "FILE", 0, "Path to the file being hidden in the cover image. "\
        "This option can be specified multiple times in order to hide more than one file. "\
        "If there is no enough space in the cover image, some files may fail being hidden "\
        "(files specified first have priority when trying to hide). "\
        "The default behavior is to overwrite the existing previously hidden files, "\
        "to avoid that add the '--apend' option.", 2},
    {"append", 'a', NULL, 0, "When hiding a file with the '--hide' option, "\
        "append the new file instead of overwriting the existing hidden files. "\
        "For this option to work, the password must be the same as the one used for the previous files", 3},
    {"password", 'p', "TEXT", 0, "Password for encrypting and scrambling the hidden data. "\
        "This option should be used alongside '--hide', '--extract', or '--check'. "\
        "The password may contain any character that your terminal allows you to input "\
        "(if it has spaces, please enclose the password between quotation marks). "\
        "If you do not want to have a password, please use '--no-password' instead of this option.", 4},
    {"no-password", 'n', NULL, 0, "Do not use a password for encrypting and scrambling the hidden data. "\
        "That means the data will be able to be extracted without needing a password.", 4},
    {"verbose", 'v', NULL, 0, "Print detailed progress information.", 5},
    {"silent", 's', NULL, 0, "Do not print any progress information (errors are still shown).", 5},
    {0}
};

// Help text to be shown above the options (when running with '--help')
static const char help_text[] = "\nSteganography tool for hiding and extracting files on JPEG and PNG images. "\
    "Multiple files can be hidden in a single cover image, "\
    "and the hidden data can be (optionally) protected with a password.\n\n"\
    "Hiding a file on an image:\n"\
    "  imgconceal --input=IMAGE --hide=FILE [--output=NEW_IMAGE] [--append] [--password=TEXT | --no-password]\n\n"\
    "Extracting a hidden file from an image:\n"\
    "  imgconceal --extract=IMAGE [--password=TEXT]\n\n"\
    "Check if an image has data hidden by this program:\n"\
    "  imgconceal --check=IMAGE [--password=TEXT]\n\n"\
    "All options:\n";

// Options and callback function for the command line interface
static const struct argp argp_struct = {argp_options, &imc_cli_parse_options, NULL, help_text};

// Internal data structure to store the user's options
typedef struct UserOptions {
    char *input;        // Path to the image which will get data hidden into it
    char *output;       // Path where to save the image with hidden data
    char *hide;         // Path to the file being hidden on theimage
    char *check;        // Path to the image being checked for hidden data
    PassBuff *password; // Plain text password provided by the user
    bool no_password;   // 'true' if not using a password
    bool verbose;       // Prints detailed information during operation
    bool silent;        // Do not print any information during operation
} UserOptions;

// Get a password from the user on the command-line. The typed characters are not displayed.
// They are stored on the 'output' buffer, up to 'buffer_size' bytes.
// Function returns the amount of bytes in the password.
static size_t __get_password(uint8_t *output, const size_t buffer_size)
{
    // Store the current settings of the terminal
    struct termios old_term, new_term;
    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;

    // Turn off input echoing
    new_term.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_term);

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

    // Turn input echoing back on
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term);

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

    return pass_1;
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

// Main callback function for the command line interface
// It receives the user's arguments, then call other parts of the program in order to perform the requested operation.
static int imc_cli_parse_options(int key, char *arg, struct argp_state *state)
{
    switch (key)
    {
        // Before parsing the options
        case ARGP_KEY_INIT:
            /* code */
            break;
        
        // If no options are passed: display a short help text
        case ARGP_KEY_NO_ARGS:
            argp_state_help(state, stdout, ARGP_HELP_PRE_DOC | ARGP_HELP_SEE);
            break;
        
        // After the last option was parsed: perform the requested operation
        case ARGP_KEY_END:
            /* code */
            break;
        
        // After the program finished the requested operation
        case ARGP_KEY_FINI:
            /* code */
            break;
    }

    return 0;
}