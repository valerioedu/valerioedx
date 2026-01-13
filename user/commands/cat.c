#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    char buf[2048];
    int n;
    
    if (argc < 2) {
        while ((n = read(0, buf, sizeof(buf))) > 0)
            write(1, buf, n);

        return 0;
    }
    
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            printf("cat: cannot access '%s': No such file or directory\n", argv[i]);
            continue;
        }
        
        while ((n = read(fd, buf, sizeof(buf))) > 0)
            write(1, buf, n);
        
        close(fd);
    }

    write(1, "\n", 1);
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