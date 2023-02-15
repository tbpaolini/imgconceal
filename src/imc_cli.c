/* Command-line interface for imgconceal */

#include "imc_includes.h"

// Get a password from the user on the command-line. The typed characters are not displayed.
// They are stored on the 'output' buffer, up to 'buffer_size' bytes.
// Function returns the amount of bytes in the password.
size_t imc_get_password(uint8_t *output, const size_t buffer_size)
{
    // Store the current settings of the terminal
    struct termios old_term, new_term;
    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;

    // Turn off input echoing
    new_term.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

    size_t pos = 0; // Position on the output buffer
    
    {
        int c = '\0';   // Current character being read
        
        while ( (c != '\r') && (c != '\n') && (pos < buffer_size) )
        {
            c = getchar();
            output[pos++] = c;
        }
    }

    // Turn input echoing back on
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);

    return pos;
}