#include "imc_includes.h"

const char *argp_program_version = "imageconceal version 1.0.1 (built on "__DATE__" at "__TIME__")\n"\
    "Copyright (c) 2023 Tiago Becerra Paolini.\n"\
    "Licensed under the MIT license.\n"\
    "https://github.com/tbpaolini/imgconceal\n"\
    "Contact: <tpaolini@gmail.com>";
const char *argp_program_bug_address = "<https://github.com/tbpaolini/imgconceal/issues> or <tpaolini@gmail.com>";

int main(int argc, char *argv[])
{
    #ifdef _WIN32
    setlocale(LC_ALL, ".utf8");
    #else // Linux
    setlocale(LC_ALL, "C.UTF-8");
    #endif // _WIN32
    
    if (sodium_init() < 0)
    {
        fprintf(stderr, "Error: Failed to initialize libsodium\n");
        exit(EXIT_FAILURE);
    }
    
    // Parse the command line arguments
    const struct argp *restrict argp_struct = imc_cli_get_argp_struct();
    return argp_parse(argp_struct, argc, argv, ARGP_IN_ORDER, NULL, NULL);
}