#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

// Built-in commands
extern int cd(char *str);
extern int pwd();

// Temporary
int rmdir_cmd(const char *path) {
    if (path == NULL) {
        printf("rmdir: missing operand\n");
        return 1;
    }
    
    int ret = rmdir(path);
    if (ret < 0) {
        printf("rmdir: failed to remove '%s'\n", path);
        return 1;
    }
    
    return 0;
}

int run_command(const char *path, char *argv[]) {
    pid_t pid = fork();
    
    if (pid == 0) {
        char *envp[] = {NULL};
        execve(path, argv, envp);
        printf("%s: exec failed\n", argv[0]);
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
    char buf[33];
    char dir[64];
    
    while (1) {
        printf("valerioedx:%s$ ", getcwd(dir, 64));
        fflush(stdout);
        fgets(buf, 32, stdin);
        buf[strcspn(buf, "\n")] = '\0';
        
        // Skip empty commands
        if (buf[0] == '\0') continue;
        
        char *saveptr;
        char *cmd = strtok_r(buf, " \t", &saveptr);
        
        if (cmd == NULL) continue;
        
        if (strcmp(cmd, "cd") == 0) {
            char *path = strtok_r(NULL, " \t", &saveptr);
            cd(path);
        } else if (strcmp(cmd, "exit") == 0) {
            break;
        } else if (strcmp(cmd, "pwd") == 0) {
            pwd();
        } else if (strcmp(cmd, "rmdir") == 0) {
            char *path = strtok_r(NULL, " \t", &saveptr);
            rmdir_cmd(path);
        } else if (strcmp(cmd, "ls") == 0) {
            char *path = strtok_r(NULL, " \t", &saveptr);
            char *argv[] = {"ls", path, NULL};
            run_command("/bin/ls", argv);
        } else if (strcmp(cmd, "cat") == 0) {
            char *path = strtok_r(NULL, " \t", &saveptr);
            char *argv[] = {"cat", path, NULL};
            run_command("/bin/cat", argv);
        } else if (strcmp(cmd, "mkdir") == 0) {
            char *path = strtok_r(NULL, " \t", &saveptr);
            char *argv[] = {"mkdir", path, NULL};
            run_command("/bin/mkdir", argv);
        } else if (strcmp(cmd, "echo") == 0) {
            char *path = strtok_r(NULL, " \t", &saveptr);
            char *argv[] = {"echo", path, NULL};
            run_command("/bin/echo", argv);
        } else if (strcmp(cmd, "touch") == 0) {
            char *path = strtok_r(NULL, " \t", &saveptr);
            char *argv[] = {"touch", path, NULL};
            run_command("/bin/touch", argv);
        } else {
            printf("Unknown command: %s\n", cmd);
        }
    }

    while (1) asm volatile("wfi");
    return 0;
}

void _start() {
    main();
}