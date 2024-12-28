#include "game.h"
#include <stdio.h>


int main(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: app port peer-port\n");
        return -1;
    }

    bool judge = argc == 4;

    Game g(atoi(argv[1]), atoi(argv[2]), judge);
    g.run();
    return 0;
}
