#include "imc_includes.h"

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, ".utf8");
    if (sodium_init() < 0)
    {
        fprintf(stderr, "Error: Failed to initialize libsodium\n");
        exit(EXIT_FAILURE);
    }

    // "C:\\Users\\tpaol\\Pictures\\17032012981.jpg"
    imc_jpeg_open_carrier("The most awesomest image ever.jpg", NULL);
    
    return 0;
}