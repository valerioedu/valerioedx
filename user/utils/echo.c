#include <unistd.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        write(1, "\n", 1);
        return 0;
    }

    for (int i = 1; i < argc - 1; i++)
        printf("%s ", argv[i]);
    
    printf("%s\n", argv[argc - 1]);
    fflush(stdout);

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