#include <stdio.h>
#include <unistd.h>

int rmdir_cmd(const char *path) {
    if (path == NULL) {
        printf("rmdir: missing operand\n");
        return -1;
    }
    
    int ret = rmdir(path);
    if (ret < 0) {
        printf("rmdir: failed to remove '%s'\n", path);
        return -1;
    }
    
    return 0;
}