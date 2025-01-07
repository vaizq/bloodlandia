#include "game.h"
#include <stdio.h>


int main(int argc, char** argv) {
    const char* addr;
    if (argc != 2) {
        printf("usage: app server-address\n");
        addr = "109.204.231.229";
    } else {
        addr = argv[1];
    }

    try {
        Game g(addr);
        g.run();
    } catch(const std::exception& e) {
        fprintf(stderr, "Game failed: %s\n", e.what());
    }
    return 0;
}
