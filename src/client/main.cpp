#include "game.h"
#include <stdio.h>


int main(int argc, char** argv) {
    std::string addr;
    if (argc != 2) {
        printf("usage: app server-address\n");
        addr = "192.168.1.22";
    } else {
        addr = std::string(argv[1]);
    }

    try {
        Game g(addr.c_str());
        g.run();
    } catch(const std::exception& e) {
        fprintf(stderr, "Game failed: %s\n", e.what());
    }
    return 0;
}
