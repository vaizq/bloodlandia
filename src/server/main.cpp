#include <stdio.h>


int main(int argc, char** argv) {
    if (argc != 2) {
        printf("usage: app port");
        return -1;
    }

    printf("server started\n");

    return 0;
}
