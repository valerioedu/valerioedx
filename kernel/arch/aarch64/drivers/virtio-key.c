#include <virtio.h>
#include <sched.h>

extern char kb_buffer[];
extern volatile int kb_head;
extern volatile int kb_tail;
extern wait_queue_t kb_wait_queue;

char virtio_kb_getchar_nonblock(void) {
    if (kb_head == kb_tail) return 0;
    
    char c = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return c;
}

// Blocking read - waits for a key
char virtio_kb_getchar(void) {
    while (kb_head == kb_tail) {
        sleep_on(&kb_wait_queue);
    }
    
    char c = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return c;
}

// Read multiple characters into buffer (blocks until newline or buffer full)
u64 virtio_kb_read(char *buf, u64 count) {
    u64 i = 0;
    while (i < count - 1) {  // Leave room for null terminator
        // Wait for input if buffer is empty
        while (kb_head == kb_tail) {
            sleep_on(&kb_wait_queue);
        }
        
        char c = kb_buffer[kb_tail];
        kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
        
        buf[i++] = c;
        
        // Return on newline (line-buffered input)
        if (c == '\n' || c == '\r') {
            break;
        }
    }
    buf[i] = '\0';
    return i;
}

u64 virtio_kb_fs_read(inode_t *node, u64 offset, u64 size, u8 *buffer) {
    (void)node; (void)offset;
    return virtio_kb_read((char*)buffer, size);
}

inode_ops virtio_kb_ops = {
    .read = virtio_kb_fs_read,
    .write = NULL,
    .open = NULL,
    .close = NULL,
    .finddir = NULL,
};