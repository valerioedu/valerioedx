#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64

// Built-in commands
extern int cd(char *str);
extern int pwd();

int run_command(const char *cmd, char *argv[]) {
    pid_t pid = fork();

    if (pid == 0) {
        char *envp[] = {NULL};
        char path[256];
        snprintf(path, sizeof(path), "/bin/%s", cmd);
        execve(path, argv, envp);
        snprintf(path, sizeof(path), "/sbin/%s", cmd);
        execve(path, argv, envp);
        snprintf(path, sizeof(path), "/usr/bin/%s", cmd);
        execve(path, argv, envp);
        snprintf(path, sizeof(path), "/usr/sbin/%s", cmd);
        execve(path, argv, envp);
        printf("command not found: %s\n", argv[0]);
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

int main() {
    char buf[MAX_CMD_LEN];
    char dir[128];
    char *argv[MAX_ARGS];
    int argc;

    while (1) {
        if (getcwd(dir, sizeof(dir)) != NULL)
            printf("valerioedx:%s$ ", dir);

        else printf("valerioedx:?$ ");
        fflush(stdout);

        if (fgets(buf, sizeof(buf), stdin) == NULL) break;
        
        buf[strcspn(buf, "\n")] = '\0';
        
        // Skip empty commands
        if (buf[0] == '\0') continue;

        argc = 0;
        char *saveptr;
        char *token = strtok_r(buf, " \t", &saveptr);
        while (token != NULL && argc < MAX_ARGS - 1) {
            argv[argc++] = token;
            token = strtok_r(NULL, " \t", &saveptr);
        }

        argv[argc] = NULL;

        if (argc == 0) continue;
        if (strcmp(argv[0], "exit") == 0)
            return 0;

        else if (strcmp(argv[0], "cd") == 0)
            cd(argv[1]);

        else if (strcmp(argv[0], "pwd") == 0)
            pwd();

        else 
            run_command(argv[0], argv);
    }

    return 0;
}

void _start() {
    asm volatile(
        "bl main\n"
        "mov x8, #1\n"
        "svc #0\n"              // _exit(return value in x0)
    );
}