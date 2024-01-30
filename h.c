#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: ./sleep_program <seconds>\n");
        return 1;
    }

    int seconds = atoi(argv[1]);

    sleep(seconds);


    return 0;
}