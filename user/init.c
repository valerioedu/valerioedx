#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

// Temporary until fork will work
// Then fork and then execve the binaries
extern int mkdir_cmd(const char *path);
extern int rmdir_cmd(const char *path);
extern int cd(char *str);
extern int pwd();
extern int ls(char *path);
extern int cat(const char *path);

int main() {
    char buf[33];
    char dir[16];
    
    while (1) {
        printf("valerioedx:%s$ ", getcwd(dir, 16));
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
        } else if (strcmp("ls", cmd) == 0) {
            char *path = strtok_r(NULL, " \t", &saveptr);
            ls(path);
        } else if (strcmp(cmd, "exit") == 0) {
            break;
        } else if (strcmp(cmd, "pwd") == 0) {
            pwd();
        } else if (strcmp(cmd, "cat") == 0) {
            char *path = strtok_r(NULL, " \t", &saveptr);
            cat(path);
        }  else if (strcmp(cmd, "mkdir") == 0) {
            char *path = strtok_r(NULL, " \t", &saveptr);
            mkdir_cmd(path);
        } else if (strcmp(cmd, "rmdir") == 0) {
            char *path = strtok_r(NULL, " \t", &saveptr);
            rmdir_cmd(path);
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