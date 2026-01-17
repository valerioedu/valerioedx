#include <stdio.h>
#include <unistd.h>

int cd(char *str) {
    if (str == NULL) {
        printf("cd: missing argument\n");
        return 1;
    }

    int ret = chdir(str);
    if (ret < 0) {
        printf("cd: no such file or directory: %s\n", str);
        return 1;
    }

    return 0;
}