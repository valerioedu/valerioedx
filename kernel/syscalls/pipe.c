#include <vfs.h>
#include <heap.h>
#include <string.h>
#include <file.h>
#include <sched.h>
#include <kio.h>
#include <file.h>
#include <sync.h>
#include <syscalls.h>

#define PIPE_BUF_SIZE 4096

typedef struct pipe {
    u8 buffer[PIPE_BUF_SIZE];
    u64 read_pos;
    u64 write_pos;
    u64 count;
    i32 readers;
    i32 writers;
    mutex_t lock;
} pipe_t;

mutex_t pipe_lock;

extern task_t *current_task;

static u64 pipe_read(inode_t *node, u64 offset, u64 size, u8 *buffer) {
    pipe_t *pipe = (pipe_t *)node->ptr;
    if (!pipe) return 0;

    if (pipe->count == 0 && pipe->writers == 0)
        return 0;

    // TODO: block if count == 0 && writers > 0 (needs wait queue / sleep)
    if (pipe->count == 0)
        return 0;

    u64 to_read = size;
    if (to_read > pipe->count)
        to_read = pipe->count;

    for (u64 i = 0; i < to_read; i++) {
        buffer[i] = pipe->buffer[pipe->read_pos];
        pipe->read_pos = (pipe->read_pos + 1) % PIPE_BUF_SIZE;
    }

    pipe->count -= to_read;
    return to_read;
}

static u64 pipe_write(inode_t *node, u64 offset, u64 size, u8 *buffer) {
    (void)offset;
    pipe_t *pipe = (pipe_t *)node->ptr;
    if (!pipe) return 0;

    // Broken pipe: no readers left
    if (pipe->readers == 0) {
        // TODO: send sigpipe
        return -1;
    }

    u64 available = PIPE_BUF_SIZE - pipe->count;

    // TODO: block if available == 0 (needs wait queue / sleep)
    if (available == 0)
        return 0;

    u64 to_write = size;
    if (to_write > available)
        to_write = available;

    for (u64 i = 0; i < to_write; i++) {
        pipe->buffer[pipe->write_pos] = buffer[i];
        pipe->write_pos = (pipe->write_pos + 1) % PIPE_BUF_SIZE;
    }

    pipe->count += to_write;
    return to_write;
}

static void pipe_close_read(inode_t *node) {
    pipe_t *pipe = (pipe_t *)node->ptr;
    if (!pipe) return;
    pipe->readers--;
    if (pipe->readers <= 0 && pipe->writers <= 0)
        kfree(pipe);
}

static void pipe_close_write(inode_t *node) {
    pipe_t *pipe = (pipe_t *)node->ptr;
    if (!pipe) return;
    pipe->writers--;
    if (pipe->readers <= 0 && pipe->writers <= 0)
        kfree(pipe);
}

static inode_ops pipe_read_ops = { 0 };
static inode_ops pipe_write_ops = { 0 };

inode_t *pipe_create_endpoint(pipe_t *pipe, int is_write) {
    inode_t *node = (inode_t *)kmalloc(sizeof(inode_t));
    if (!node) return NULL;

    memset(node, 0, sizeof(inode_t));
    strcpy(node->name, is_write ? "pipe_w" : "pipe_r");
    node->flags = FS_FILE | FS_TEMPORARY;
    node->mode  = S_IRUSR | S_IWUSR;
    node->size  = 0;
    node->ptr   = pipe;
    node->ref_count = 1;
    node->ops   = is_write ? &pipe_write_ops : &pipe_read_ops;

    return node;
}

i64 sys_pipe(int fd[2]) {
    if (!current_task || !current_task->proc)
        return -1;

    int kfd[2];

    pipe_t *pipe = (pipe_t *)kmalloc(sizeof(pipe_t));
    if (!pipe) return -1;

    memset(pipe, 0, sizeof(pipe_t));
    pipe->readers = 1;
    pipe->writers = 1;

    inode_t *read_end  = pipe_create_endpoint(pipe, 0);
    inode_t *write_end = pipe_create_endpoint(pipe, 1);
    if (!read_end || !write_end) {
        if (read_end)  kfree(read_end);
        if (write_end) kfree(write_end);

        kfree(pipe);
        return -1;
    }

    kfd[0] = fd_alloc();
    file_t *file1 = file_new(read_end, 1);
    current_task->proc->fd_table[kfd[0]] = file1;
    if (kfd[0] < 0) {
        kfree(read_end);
        kfree(write_end);
        kfree(pipe);
        return -1;
    }

    kfd[1] = fd_alloc();
    file_t *file2 = file_new(write_end, 1);
    current_task->proc->fd_table[kfd[1]] = file2;
    if (kfd[1] < 0) {
        fd_free(kfd[0]);
        kfree(write_end);
        kfree(pipe);
        return -1;
    }

    extern int copy_to_user(void *user_dst, const void *kernel_src, size_t size);
    if (copy_to_user(fd, kfd, sizeof(kfd)) != 0) {
        fd_free(kfd[0]);
        fd_free(kfd[1]);
        return -1;
    }

    return 0;
}

void pipe_init() {
    pipe_write_ops.write = pipe_write;
    pipe_write_ops.close = pipe_close_write;

    pipe_read_ops.read = pipe_read;
    pipe_read_ops.close = pipe_close_read;

    mutex_init(&pipe_lock);
}