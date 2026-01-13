#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("rmdir: missing operand\n");
        return -1;
    }
    
    int ret = rmdir(argv[1]);
    if (ret < 0) {
        printf("rmdir: failed to remove '%s'\n", argv[1]);
        return -1;
    }
    
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