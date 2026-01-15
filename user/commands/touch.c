#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: touch <filename>\n");
        return 1;
    }

    int fd = open(argv[1], O_CREAT | O_RDWR);
    
    if (fd < 0) {
        printf("touch: cannot create file '%s'\n", argv[1]);
        return 1;
    }

    close(fd);
    return 0;
}

__attribute__((naked)) void _start() {
    asm volatile(
        "ldr x0, [sp]\n"        // x0 = argc
        "add x1, sp, #8\n"      // x1 = &argv[0]
        "bl main\n"
        "mov x8, #1\n"
        "svc #0\n"              // _exit(return value in x0)
    );
}