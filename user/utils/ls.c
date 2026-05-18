#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>

static const int days_in_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static int is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int days_in_year(int year) {
    return is_leap_year(year) ? 366 : 365;
}

static void unix_to_datetime(uint64_t timestamp, int *year, int *month, int *day,
                              int *hour, int *min, int *sec) {
    uint64_t rem = timestamp;
    
    *sec = rem % 60; rem /= 60;
    *min = rem % 60; rem /= 60;
    *hour = rem % 24; rem /= 24;
    
    int y = 1970;
    while (rem >= (uint64_t)days_in_year(y)) {
        rem -= days_in_year(y);
        y++;
    }

    *year = y;
    
    int m = 0;
    while (m < 11) {
        int dim = days_in_month[m];
        if (m == 1 && is_leap_year(y)) dim++;
        if (rem < (uint64_t)dim) break;
        rem -= dim;
        m++;
    }

    *month = m + 1;
    *day = rem + 1;
}

int main(int argc, char *argv[]) {
    const char *path = ".";
    bool lflag = false;
    bool aflag = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            lflag = true;
        } else if (strcmp(argv[i], "-a") == 0) {
            aflag = true;
        } else if (strcmp(argv[i], "-la") == 0) {
            lflag = aflag = true;
        } else if (strcmp(argv[i], "-al") == 0) {
            lflag = aflag = true;
        } else {
            path = argv[i];
        }
    }
    
    int fd = open(path, 0);
    if (fd < 0) {
        printf("ls: cannot access '%s': No such file or directory\n", path);
        return 1;
    }
    
    char buf[1024];
    int64_t basep = 0;
    ssize_t bytes_read;
    
    while ((bytes_read = getdirentries(fd, buf, sizeof(buf), &basep)) > 0) {
        char *ptr = buf;
        
        while (ptr < buf + bytes_read) {
            struct dirent *entry = (struct dirent*)ptr;
            
            // Skip entries with inode 0
            if (entry->d_ino != 0) {
                if (!aflag && (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)) {
                    ptr += entry->d_reclen;
                    continue;
                }

                char fullpath[1024];
                snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
                
                struct stat st;
                if (stat(fullpath, &st) == 0 && lflag) {
                    int year, month, day, hour, min, sec;
                    unix_to_datetime(st.st_mtime, &year, &month, &day, &hour, &min, &sec);
                    printf("%s \t%d-%d-%d %d:%d:%d\n", entry->d_name, year, month, day, hour, min, sec);
                } else {
                    printf("%s\n", entry->d_name);
                }
            }
            
            ptr += entry->d_reclen;
        }
    }
    
    close(fd);
    return 0;
}