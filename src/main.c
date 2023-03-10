#include "imc_includes.h"

const char *argp_program_version = "imageconceal version 1.0.0\n"\
    "Copyright (c) 2023 Tiago Becerra Paolini.\n"\
    "Licensed under the MIT license.\n"\
    "https://github.com/tbpaolini/imgconceal";
const char *argp_program_bug_address = "<https://github.com/tbpaolini/imgconceal/issues>";

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