#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>

int main() {
    pid_t pid = fork();

    if (pid == 0) {
        char *argv[] = {"sh", NULL};
        char *envp[] = {NULL};
        
        execve("/bin/sh", argv, envp);
        _exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return status;
    } else {
        printf("fork failed\n");
        return -1;
    }

    while (1) {
        int status;
        pid_t zombie = wait(&status);

        if (zombie > 0) {
            // TODO: Print a log if the shell itself crashes
            if (zombie == pid) {
                //TODO: Implement login and launch it here
                printf("init: shell exited (pid %d).\n", zombie);
                pid_t pid = fork();

                if (pid == 0) {
                    char *argv[] = {"sh", NULL};
                    char *envp[] = {NULL};
                    
                    execve("/bin/sh", argv, envp);
                    _exit(1);
                } else if (pid > 0) {
                    int status;
                    waitpid(pid, &status, 0);
                    return status;
                } else {
                    printf("fork failed\n");
                    return -1;
                }
            }
        } else  asm volatile("wfi");
    }

    return 0;
}

void _start() {
    main();
}