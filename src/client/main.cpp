#include "game.h"
#include <stdio.h>


int main(int argc, char** argv) {
    if (argc != 2) {
        printf("usage: app server-address\n");
        return -1;
    }

    try {
        Game g(argv[1]);
        g.run();
    } catch(const std::exception& e) {
        fprintf(stderr, "Game failed: %s\n", e.what());
    }
    return 0;
}
