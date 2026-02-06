#include <sys/reboot.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    if (getuid() != 0) {
        fprintf(stderr, "reboot: Operation not permitted\n");
        return 1;
    }

    if (reboot(RB_AUTOBOOT) == 0) {
        fprintf(stderr, "shutdown: Error\n");
        return 1;
    }

    return 0;
}