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
    // imc_jpeg_open_carrier("The most awesomest image ever.jpg", NULL);
    // imc_jpeg_open_carrier("17032012981.jpg", NULL);
    CarrierImage *test;
    imc_steg_init(
        "/home/tiago/proj/imgconceal/bin/linux/debug/The most awesomest image ever.jpg",
        (argc >= 2) ? argv[1] : "Tiago",
        &test
    );
    imc_steg_insert(test, "/home/tiago/proj/imgconceal/bin/linux/debug/pintor-barroco-desconhecido-jesus-misecordioso-d.jpg");
    imc_steg_finish(test, "teste.jpg");
    
    return 0;
}