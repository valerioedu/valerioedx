#include <unistd.h>
#include <string.h>

int main() {
    char buf[33];
    char dir[16];

    char *argv[] = {"/usr/bin/echo.elf", "Hi", NULL};
    char *envp[] = {NULL};
    
    execve("/usr/bin/echo.elf", argv, envp);
    
    // This code only runs if execve fails
    write(1, "execve failed\n", 14);
    while (1) {
        write(1, "valerioedx:", strlen("valerioedx:"));
        getcwd(dir, 2);
        int len = 0;
        while (dir[len] != '\0' && len < 16) len++;
        dir[len++] = '$';
        dir[len++] = ' ';
        write(1, dir, len);
        read(0, buf, 32);
        write(1, "\n", 1);
    }

    while (1) asm volatile("wfi");
    return 0;
}

void _start() {
    main();
}