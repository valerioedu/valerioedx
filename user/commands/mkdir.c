#include <stdio.h>
#include <sys/stat.h>

int mkdir_cmd(const char *path) {
    if (path == NULL) {
        printf("mkdir: missing operand\n");
        return -1;
    }
    
    int ret = mkdir(path, 0755);
    if (ret < 0) {
        printf("mkdir: cannot create directory '%s'\n", path);
        return -1;
    }
    
    return 0;
}