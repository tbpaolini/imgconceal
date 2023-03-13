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
    char *extract;      // Path to the image with hidden data being extracted
    char *check;        // Path to the image being checked for hidden data
    struct HideList {
        char *data;
        struct HideList *next;
    } hide;             // Linked list with the paths to the files being hidden on the image
    struct HideList *hide_tail; // Last element of the 'hide' linked list
    PassBuff *password; // Plain text password provided by the user
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

// Store a copy of the path of a file
static inline void __store_path(const char *path, char **destination)
{
    if (path) *destination = strdup(path);
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
    
    // Initialize the steganography data structure
    // (generate a secret key and seed the pseudo-random number generator)
    steg_status = imc_steg_init(steg_path, opt->password, &steg_image);
    imc_cli_password_free( ((UserOptions*)(state->hook))->password );

    switch (steg_status)
    {
        case IMC_SUCCESS:
            break;
        
        case IMC_ERR_FILE_NOT_FOUND:
            argp_failure(state, EXIT_FAILURE, 0, "file '%s' was not found.", steg_path);
            break;
        
        case IMC_ERR_FILE_INVALID:
            argp_failure(state, EXIT_FAILURE, 0, "file '%s' is not a valid JPEG or PNG image.", steg_path);
            break;
        
        case IMC_ERR_NO_MEMORY:
            argp_failure(state, EXIT_FAILURE, 0, "no enough memory for hashing the password.");
            break;
        
        default:
            argp_failure(state, EXIT_FAILURE, 0, "unknown error when hashing the password.");
            break;
    }

    /* TO DO: Operation on the image */

    // Path where to save the output image
    char *save_path = NULL;

    if (mode == HIDE)
    {
        save_path = opt->output ? opt->output : opt->input;
        /* Note: The input image will not be overwritten because our file name
           collision resolution is going to append a number to the output's name. */
    }

    // Save the output image (if any) and free the memory used by the steganography struct
    imc_steg_finish(steg_image, save_path);
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
    
    // Handle the Argp's events
    switch (key)
    {
        // Before parsing the options: allocate memory for storing the options
        case ARGP_KEY_INIT:
            state->hook = imc_calloc(1, sizeof(UserOptions));
            break;
        
        // --check: Image to be checked for hidden data
        case 'c':
            __check_unique_option(state, "check", ((UserOptions*)(state->hook))->check);
            __store_path(arg, &((UserOptions*)(state->hook))->check);
            break;
        
        // --extract: Image to have its hidden data extracted
        case 'e':
            __check_unique_option(state, "extract", ((UserOptions*)(state->hook))->extract);
            __store_path(arg, &((UserOptions*)(state->hook))->extract);
            break;
        
        // --input: Image to get data hidden into it
        case 'i':
            __check_unique_option(state, "input", ((UserOptions*)(state->hook))->input);
            __store_path(arg, &((UserOptions*)(state->hook))->input);
            break;
        
        // --output: Where to save the image with hidden data
        case 'o':
            __check_unique_option(state, "output", ((UserOptions*)(state->hook))->output);
            __store_path(arg, &((UserOptions*)(state->hook))->output);
            break;
        
        // --hide: File being hidden on the image
        case 'h':
            struct HideList **tail = &((UserOptions*)(state->hook))->hide_tail;
            
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
                __store_path(arg, &((UserOptions*)(state->hook))->hide.data);
                *tail = &((UserOptions*)(state->hook))->hide;
            }
            
            break;
        
        // --append: If the file being hidden is going to be appended to existing ones
        case 'a':
            ((UserOptions*)(state->hook))->append = true;
            break;
        
        // --password: Password provided by the user
        case 'p':
            if (((UserOptions*)(state->hook))->no_password)
            {
                argp_error(state, "you provided a password even though you specified the 'no password' option.");
            }
            
            // Create a password buffer and copy the string to it
            {
                PassBuff *user_password = __alloc_passbuff();
                user_password->length = strlen(arg);
                strncpy(user_password->buffer, arg, IMC_PASSWORD_MAX_BYTES);
                if (user_password->length > IMC_PASSWORD_MAX_BYTES) user_password->length = IMC_PASSWORD_MAX_BYTES;
                ((UserOptions*)(state->hook))->password = user_password;
            }
            
            break;
        
        // --no-password: Do not show a password prompt if the user has not provided a password
        case 'n':
            if (((UserOptions*)(state->hook))->password)
            {
                argp_error(state, "you provided a password even though you specified the 'no password' option.");
            }
            ((UserOptions*)(state->hook))->no_password = true;
            ((UserOptions*)(state->hook))->password = __alloc_passbuff();   // Store an empty password
            break;
        
        // --verbose: Prints detailed information during operation
        case 'v':
            ((UserOptions*)(state->hook))->verbose = true;
            break;
        
        // --silent: Do not print detailed information
        case 's':
            ((UserOptions*)(state->hook))->silent = true;
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
            free( ((UserOptions*)(state->hook))->check );
            free( ((UserOptions*)(state->hook))->extract );
            free( ((UserOptions*)(state->hook))->input );
            free( ((UserOptions*)(state->hook))->output );

            // Freeing the linked list
            {
                free( ((UserOptions*)(state->hook))->hide.data );
                struct HideList *node = ((UserOptions*)(state->hook))->hide.next;
                while (node)
                {
                    struct HideList *next_node = node->next;
                    imc_free(node);
                    node = next_node;
                };
            }

            imc_free(state->hook);
            break;
    }

    return 0;
}