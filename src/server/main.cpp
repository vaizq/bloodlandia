#include <stdio.h>
#include "server.h"





int main(int argc, char** argv) 
{
    if (argc != 2) {
        printf("usage: app port\n");
        return -1;
    }

    Server server{6969, 6942};

    for(;;) {

        auto& conns = server.getConnections();
        for (Connection& conn : conns) {
        }

        server.poll();
    }

    return 0;
}
