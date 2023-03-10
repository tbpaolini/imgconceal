#include "imc_includes.h"

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, ".utf8");
    if (sodium_init() < 0)
    {
        fprintf(stderr, "Error: Failed to initialize libsodium\n");
        exit(EXIT_FAILURE);
    }
    
    // Parse the command line arguments
    const struct argp *restrict argp_struct = imc_cli_get_argp_struct();
    return argp_parse(argp_struct, argc, argv, ARGP_NO_ARGS, NULL, NULL);
}