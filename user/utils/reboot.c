#include <sys/reboot.h>
#include <unistd.h>
#include <stdio.h>

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

void _start() {
    asm volatile(
        "ldr x0, [sp]\n"        // x0 = argc
        "add x1, sp, #8\n"      // x1 = &argv[0]
        "bl main\n"
        "mov x8, #1\n"
        "svc #0\n"              // _exit(return value in x0)
    );
}