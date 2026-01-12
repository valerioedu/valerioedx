#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

int cat(const char *path) {
    //if (path == NULL) read(0, 0, BUFSIZ);
    char buf[2048];
    int fd = open(path, 0);
    if (fd < 0) {
        printf("cat: cannot access '%s': No such file or directory\n", path);
        return -1;
    }
    
    read(fd, buf, 2048);
    printf("%s\n", buf);
    close(fd);
    return 0;
}