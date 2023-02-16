/* Command-line interface for imgconceal */

#include "imc_includes.h"

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
PassBuff *imc_cli_password_input(bool confirm)
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
void imc_cli_password_free(PassBuff *password)
{
    sodium_free(password);
    // Note: the above function already overwrites the memory before freeing it.
}