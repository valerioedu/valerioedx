#include <stdio.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <string.h>

//TODO: Implement init signal and -o flag

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s -h|-r\n", argv[0]);
        return 1;
    }

    if (getuid() != 0) {
        fprintf(stderr, "shutdown: Operation not permitted\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        printf("looping\n");
        if (strcmp(argv[i], "-h") == 0) {
            if (reboot(RB_POWEROFF) == -1) {
                fprintf(stderr, "shutdown: Error\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-r") == 0) {
            if (reboot(RB_AUTOBOOT) == 0) {
                fprintf(stderr, "shutdown: Error\n");
                return 1;
            }
        } else {
            fprintf(stderr, "shutdown: illegal option: %s\n", argv[i]);
            return 1;
        }
    }

    printf("done");
    return 0;
}