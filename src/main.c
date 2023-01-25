#include "imc_includes.h"

// Store the byte order of the system
bool IS_LITTLE_ENDIAN;

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, ".utf8");
    if (sodium_init() < 0)
    {
        fprintf(stderr, "Error: Failed to initialize libsodium\n");
        exit(EXIT_FAILURE);
    }

    // Verify whether the system is little or big endian
    {
        const uint16_t val = 1;
        const uint8_t *bytes = (uint8_t *)(&val);
        IS_LITTLE_ENDIAN = bytes[0] == 1 ? true : false;
    }

    // "C:\\Users\\tpaol\\Pictures\\17032012981.jpg"
    imc_jpeg_open_carrier("The most awesomest image ever.jpg", NULL);
    
    return 0;
}