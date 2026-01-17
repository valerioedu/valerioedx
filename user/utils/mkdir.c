#include <stdio.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("mkdir: missing operand\n");
        return 1;
    }
    
    struct stat st;
    if (stat(argv[1], &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            printf("mkdir: cannot create directory '%s': File exists\n", argv[1]);
            return 1;
        } else {
            printf("mkdir: cannot create directory '%s': File exists and is not a directory\n", argv[1]);
            return 1;
        }
    }

    int ret = mkdir(argv[1], 0755);
    if (ret < 0) {
        printf("mkdir: cannot create directory '%s'\n", argv[1]);
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