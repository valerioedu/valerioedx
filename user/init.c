#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

int main() {
    char buf[33];
    char dir[16];

    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process*/
        char *argv[] = {"/bin/echo.elf", "Hi", NULL};
        char *envp[] = {NULL};
        execve("/bin/echo.elf", argv, envp);
        // Only reaches here if execve fails
        write(1, "execve failed\n", 14);
        _exit(1);
    } else {
        int status;
        write(1, "waiting\n", strlen("waiting\n"));
        wait(&status);
        write(1, "Wait complete\n", strlen("Wait complete\n"));
    }

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