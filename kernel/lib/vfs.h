#ifndef VFS_H
#define VFS_H

#include <lib.h>

#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_CHARDEVICE  0x03
#define FS_BLOCKDEVICE 0x04
#define FS_TEMPORARY   0x80

#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100
#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010
#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001
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

#define vfs_lookup(path) namei(path)    // For legacy compatibility

struct vfs_node;

// Drivers implement these specific functions.
typedef struct {
    u64 (*read) (struct vfs_node* file, u64 format, u64 size, u8* buffer);
    u64 (*write)(struct vfs_node* file, u64 format, u64 size, u8* buffer);
    void (*open) (struct vfs_node* file);
    void (*close)(struct vfs_node* file);
    struct vfs_node* (*lookup)(struct vfs_node* file, const char *name);
    struct vfs_node* (*create)(struct vfs_node*, const char*);      // Create file
    struct vfs_node* (*mkdir)(struct vfs_node*, const char*);       // Create directory
    int (*unlink)(struct vfs_node*, const char*);                // Delete file
    int (*rmdir)(struct vfs_node*, const char*);                 // Delete directory
    struct vfs_node* (*symlink)(struct vfs_node*, const char*, const char*);  // Create symlink
    int (*readdir)(struct vfs_node*, int, char *namebuf, int buflen, int *isdir);
    int (*ioctl)(struct vfs_node* file, u64 request, u64 arg);
} inode_ops;

/*
Linux operations, at the end inode_ops should look something like this

struct inode_operations {
    struct file_operations * default_file_ops;
    int (*create) (struct inode *,const char *,int,int,struct inode **);
    int (*lookup) (struct inode *,const char *,int,struct inode **);
    int (*link) (struct inode *,struct inode *,const char *,int);
    int (*unlink) (struct inode *,const char *,int);
    int (*symlink) (struct inode *,const char *,int,const char *);
    int (*mkdir) (struct inode *,const char *,int,int);
    int (*rmdir) (struct inode *,const char *,int);
    int (*mknod) (struct inode *,const char *,int,int,int);
    int (*rename) (struct inode *,const char *,int, struct inode *,
               const char *,int, int);
    int (*readlink) (struct inode *,char *,int);
    int (*follow_link) (struct inode *,struct inode *,int,int,struct inode **);
    int (*readpage) (struct inode *, struct page *);
    int (*writepage) (struct inode *, struct page *);
    int (*bmap) (struct inode *,int);
    void (*truncate) (struct inode *);
    int (*permission) (struct inode *, int);
    int (*smap) (struct inode *,int);
};

struct file_operations {
    int (*lseek) (struct inode *, struct file *, off_t, int);
    int (*read) (struct inode *, struct file *, char *, int);
    int (*write) (struct inode *, struct file *, const char *, int);
    int (*readdir) (struct inode *, struct file *, void *, filldir_t);
    int (*select) (struct inode *, struct file *, int, select_table *);
    int (*ioctl) (struct inode *, struct file *, unsigned int, unsigned long);
    int (*mmap) (struct inode *, struct file *, struct vm_area_struct *);
    int (*open) (struct inode *, struct file *);
    void (*release) (struct inode *, struct file *);
    int (*fsync) (struct inode *, struct file *);
    int (*fasync) (struct inode *, struct file *, int);
    int (*check_media_change) (kdev_t dev);
    int (*revalidate) (kdev_t dev);
};*/

typedef struct vfs_node {
    char name[64];
    u32 flags;
    u32 uid;
    u32 gid;
    u32 mode;
    u64 size;
    u64 id;
    u32 nlink;
    u32 blksize;        // Preferred block size for I/O
    u64 blocks;
    u64 dev;
    u64 rdev;
    inode_ops *ops;
    void *ptr;             // Private driver data
    i32 ref_count;
    struct vfs_node *mount_point;
    struct vfs_node *parent;
} inode_t;

extern inode_t *vfs_root;

void vfs_init();
void vfs_retain(inode_t *node);
u64 vfs_read(inode_t* node, u64 offset, u64 size, u8* buffer);
u64 vfs_write(inode_t* node, u64 offset, u64 size, u8* buffer);
void vfs_open(inode_t* node);
void vfs_close(inode_t* node);
inode_t *vfs_create(inode_t *node, const char *name);
int vfs_unlink(struct vfs_node *parent, const char *name);

void vfs_mount_root(inode_t *node);
int vfs_mount(inode_t* mountpoint, inode_t* fs_root);
inode_t *vfs_lookup(const char *path);

inode_t* devfs_fetch_device(const char* name);
void devfs_mount_device(char* name, inode_ops* ops, int type);
void devfs_init();
inode_t *devfs_get_root();
inode_t *namei(const char *path);

#endif