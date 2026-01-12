#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

int ls(char *path) {
    if (path == NULL) {
        path = ".";
    }
    
    int fd = open(path, 0);
    if (fd < 0) {
        printf("ls: cannot access '%s': No such file or directory\n", path);
        return -1;
    }
    
    char buf[1024];
    int64_t basep = 0;
    ssize_t bytes_read;
    
    while ((bytes_read = getdirentries(fd, buf, sizeof(buf), &basep)) > 0) {
        char *ptr = buf;
        
        while (ptr < buf + bytes_read) {
            struct dirent *entry = (struct dirent*)ptr;
            
            // Skip entries with inode 0
            if (entry->d_ino != 0)
                printf("%s\n", entry->d_name);
            
            ptr += entry->d_reclen;
        }
    }
    
    close(fd);
    return 0;
}