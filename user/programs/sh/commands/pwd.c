#include <unistd.h>
#include <stdio.h>

int pwd() {
    char ret[64];
    char *result = getcwd(ret, 64);
    if (result == NULL) {
        printf("Error: could not retrieve cwd\n");
        return -1;
    }

    printf("%s\n", ret);
    return 0;
}