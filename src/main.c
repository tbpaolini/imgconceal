#include "imc_includes.h"

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, ".utf8");
    if (sodium_init() < 0)
    {
        fprintf(stderr, "Error: Failed to initialize libsodium\n");
        exit(EXIT_FAILURE);
    }
    
    return argp_parse(NULL, argc, argv, 0, NULL, NULL);
}