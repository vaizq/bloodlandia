#include "game.h"
#include <stdio.h>


int main(int argc, char** argv) {
    if (argc != 2) {
        printf("usage: app server-address\n");
        return -1;
    }

    Game g(argv[1]);
    g.run();
    return 0;
}
