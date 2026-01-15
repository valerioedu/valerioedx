#include <syscalls.h>
#include <vfs.h>
#include <file.h>
#include <sched.h>
#include <heap.h>
#include <string.h>

#define S_IFMT   0170000  // Mask for file type
#define S_IFSOCK 0140000  // Socket
#define S_IFLNK  0120000  // Symbolic link
#define S_IFREG  0100000  // Regular file
#define S_IFBLK  0060000  // Block device
#define S_IFDIR  0040000  // Directory
#define S_IFCHR  0020000  // Character device
#define S_IFIFO  0010000  // FIFO

#define S_IRWXU  00700    // Owner RWX
#define S_IRUSR  00400    // Owner R
#define S_IWUSR  00200    // Owner W
#define S_IXUSR  00100    // Owner X
#define S_IRWXG  00070    // Group RWX
#define S_IRGRP  00040    // Group R
#define S_IWGRP  00020    // Group W
#define S_IXGRP  00010    // Group X
#define S_IRWXO  00007    // Others RWX
#define S_IROTH  00004    // Others R
#define S_IWOTH  00002    // Others W
#define S_IXOTH  00001    // Others X

#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

typedef struct stat {
    u64 st_dev;      // Device ID
    u64 st_ino;      // Inode number
    u32 st_mode;     // File mode (type + permissions)
    u32 st_nlink;    // Number of hard links
    u32 st_uid;      // Owner user ID
    u32 st_gid;      // Owner group ID
    u64 st_rdev;     // Device ID (if special file)
    i64 st_size;     // Total size in bytes
    i64 st_blksize;  // Block size for I/O
    i64 st_blocks;   // Number of 512B blocks allocated
    i64 st_atime;    // Last access time
    i64 st_mtime;    // Last modification time
    i64 st_ctime;    // Last status change time
} stat_t;

extern task_t *current_task;

// Convert VFS flags to stat mode
static u32 vfs_flags_to_mode(u32 vfs_flags) {
    u32 mode = 0;
    
    switch (vfs_flags & 0x0F) {
        case FS_FILE:
            mode = S_IFREG;
            break;
        case FS_DIRECTORY:
            mode = S_IFDIR;
            break;
        case FS_CHARDEVICE:
            mode = S_IFCHR;
            break;
        case FS_BLOCKDEVICE:
            mode = S_IFBLK;
            break;
        default:
            mode = S_IFREG;
            break;
    }
    
    // Default permissions: rwxr-xr-x for dirs, rw-r--r-- for files
    if (mode == S_IFDIR)
        mode |= S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    else
        mode |= S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    
    return mode;
}

static int fill_stat(inode_t *node, stat_t *st) {
    if (!node || !st) return -1;
    
    memset(st, 0, sizeof(stat_t));
    
    //TODO: Implement device ids
    st->st_dev = 0;
    st->st_ino = node->id;
    st->st_mode = vfs_flags_to_mode(node->flags);
    // FAT32 doesn't support hard links
    st->st_nlink = 1;
    st->st_uid = 0;                          // Root user
    st->st_gid = 0;                          // Root group
    st->st_rdev = 0;                         // Device ID for special files
    st->st_size = node->size;
    st->st_blksize = 512;                    // Optimal I/O block size
    st->st_blocks = (node->size + 511) / 512;
    
    //TODO: Implement filestamps
    st->st_atime = 0;
    st->st_mtime = 0;
    st->st_ctime = 0;
    
    return 0;
}

i64 sys_stat(const char *path, stat_t *statbuf) {
    if (!path || !statbuf) return -1;
    if (!current_task || !current_task->proc) return -1;
    
    char *kpath = kmalloc(256);
    if (!kpath) return -1;
    
    if (copy_from_user(kpath, path, 256) != 0) {
        kfree(kpath);
        return -1;
    }
    
    inode_t *node = namei(kpath);
    kfree(kpath);
    
    if (!node) return -1;
    
    stat_t kstat;
    int ret = fill_stat(node, &kstat);
    
    vfs_close(node);
    
    if (ret != 0) return -1;
    
    if (copy_to_user(statbuf, &kstat, sizeof(stat_t)) != 0)
        return -1;
    
    return 0;
}

i64 sys_fstat(int fd, stat_t *statbuf) {
    if (!statbuf) return -1;
    if (!current_task || !current_task->proc) return -1;
    
    file_t *file = fd_get(fd);
    if (!file || !file->inode) return -1;
    
    stat_t kstat;
    int ret = fill_stat(file->inode, &kstat);
    
    if (ret != 0) return -1;
    
    if (copy_to_user(statbuf, &kstat, sizeof(stat_t)) != 0)
        return -1;
    
    return 0;
}

//TODO: Update lstat to not use symlinks when implemented
i64 sys_lstat(const char *path, stat_t *statbuf) {
    return sys_stat(path, statbuf);
}