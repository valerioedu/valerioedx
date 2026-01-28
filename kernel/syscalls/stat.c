#include <syscalls.h>
#include <vfs.h>
#include <file.h>
#include <sched.h>
#include <heap.h>
#include <string.h>

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
    switch (vfs_flags & 0x0F) {
        case FS_FILE:        return S_IFREG;
        case FS_DIRECTORY:   return S_IFDIR;
        case FS_CHARDEVICE:  return S_IFCHR;
        case FS_BLOCKDEVICE: return S_IFBLK;
        default:             return S_IFREG;
    }
}

static int fill_stat(inode_t *node, stat_t *st) {
    if (!node || !st) return -1;
    
    memset(st, 0, sizeof(stat_t));
    
    st->st_dev = node->dev;
    st->st_ino = node->id;
    
    if ((node->mode & S_IFMT) != 0) {
        st->st_mode = node->mode;
    } else {
        st->st_mode = vfs_flags_to_mode(node->flags) | (node->mode & 07777);
    }

    // FAT32 doesn't support hard links
    st->st_nlink = (node->nlink > 0) ? node->nlink : 1;
    st->st_uid = node->uid;                     // Root user
    st->st_gid = node->gid;                     // Root group
    st->st_rdev = node->rdev;                   // Device ID for special files
    st->st_size = node->size;
    st->st_blksize = (node->blksize > 0) ? node->blksize : 512;
    if (node->blocks > 0) {
        st->st_blocks = node->blocks;
    } else {
        st->st_blocks = (node->size + 511) / 512;
    }
    
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