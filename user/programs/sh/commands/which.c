#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

int which(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: whcih <filename>\n");
        return 1;
    }

    if (strcmp("which", argv[1]) == 0) {
        printf("which: shell built-in command\n");
        return 0;
    } else if (strcmp("cd", argv[1]) == 0) {
        printf("cd: shell built-in command\n");
        return 0;
    } else if (strcmp("pwd", argv[1]) == 0) {
        printf("pwd: shell built-in command\n");
        return 0;
    } else if (strcmp("export", argv[1]) == 0) {
        printf("export: shell reserved word\n");
        return 0;
    }

    struct stat st;
    char path[256];
    snprintf(path, sizeof(path), "/bin/%s", argv[1]);
    if (stat(path, &st) == 0) { 
        printf("%s\n", path);
        return 0;
    }
        snprintf(path, sizeof(path), "/sbin/%s", argv[1]);
    if (stat(path, &st) == 0) { 
        printf("%s\n", path);
        return 0;
    }
        snprintf(path, sizeof(path), "/usr/bin/%s", argv[1]);
        if (stat(path, &st) == 0) { 
        printf("%s\n", path);
        return 0;
    }
        snprintf(path, sizeof(path), "/usr/sbin/%s", argv[1]);
    if (stat(path, &st) == 0) { 
        printf("%s\n", path);
        return 0;
    }
    
    fprintf(stderr, "%s not found\n", argv[1]);
    return 1;
}