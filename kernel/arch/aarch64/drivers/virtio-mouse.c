#include <virtio.h>
#include <sched.h>
#include <string.h>

#define MOUSE_BUFFER_SIZE 128

extern virtio_input_event *input_events;

typedef struct {
    i32 x;
    i32 y;
    i32 rel_x;
    i32 rel_y;
    u8 buttons;       // bit0=left, bit1=right, bit2=middle
    volatile int ready;
} mouse_state_t;

mouse_state_t mouse_state = { 0, 0, 0, 0, 0, 0 };

typedef struct {
    i32 rel_x;
    i32 rel_y;
    u8 buttons;
} mouse_event_t;

static mouse_event_t mouse_buffer[MOUSE_BUFFER_SIZE];
static volatile int mouse_head = 0;
static volatile int mouse_tail = 0;
wait_queue_t mouse_wait_queue = NULL;

void virtio_mouse_push_event(void) {
    int next_head = (mouse_head + 1) % MOUSE_BUFFER_SIZE;
    if (next_head != mouse_tail) {
        mouse_buffer[mouse_head].rel_x = mouse_state.rel_x;
        mouse_buffer[mouse_head].rel_y = mouse_state.rel_y;
        mouse_buffer[mouse_head].buttons = mouse_state.buttons;
        mouse_head = next_head;
        
        // Reset relative movement after queuing
        mouse_state.rel_x = 0;
        mouse_state.rel_y = 0;
        
        wake_up(&mouse_wait_queue);
    }
}

void virtio_mouse_handle_event(virtio_input_event *evt) {
    if (evt->type == EV_REL) {
        if (evt->code == REL_X) {
            mouse_state.rel_x += (i32)evt->value;
            mouse_state.x += (i32)evt->value;
        } else if (evt->code == REL_Y) {
            mouse_state.rel_y += (i32)evt->value;
            mouse_state.y += (i32)evt->value;
        }
    } else if (evt->type == EV_KEY) {
        u8 bit = 0;
        if (evt->code == BTN_LEFT) bit = 0x01;
        else if (evt->code == BTN_RIGHT) bit = 0x02;
        else if (evt->code == BTN_MIDDLE) bit = 0x04;
        
        if (bit) {
            if (evt->value == 1)
                mouse_state.buttons |= bit;

            else if (evt->value == 0)
                mouse_state.buttons &= ~bit;
        }
    } else if (evt->type == EV_SYN && evt->code == SYN_REPORT)
        virtio_mouse_push_event();
}

u64 virtio_mouse_read(char *buf, u64 count) {
    if (count < sizeof(mouse_event_t)) return 0;
    
    while (mouse_head == mouse_tail)
        sleep_on(&mouse_wait_queue, NULL);
    
    u64 bytes = 0;
    while (bytes + sizeof(mouse_event_t) <= count && mouse_head != mouse_tail) {
        memcpy(buf + bytes, &mouse_buffer[mouse_tail], sizeof(mouse_event_t));
        mouse_tail = (mouse_tail + 1) % MOUSE_BUFFER_SIZE;
        bytes += sizeof(mouse_event_t);
    }
    
    return bytes;
}

u64 virtio_mouse_fs_read(inode_t *node, u64 offset, u64 size, u8 *buffer) {
    return virtio_mouse_read((char*)buffer, size);
}

int virtio_mouse_ioctl(inode_t *node, u64 request, u64 arg) {
    // IOCTL 0x01: Get absolute position
    if (request == 0x01 && arg) {
        i32 *pos = (i32*)arg;
        pos[0] = mouse_state.x;
        pos[1] = mouse_state.y;
        return 0;
    }
    // IOCTL 0x02: Get button state
    if (request == 0x02 && arg) {
        *(u8*)arg = mouse_state.buttons;
        return 0;
    }
    // IOCTL 0x03: Set absolute position
    if (request == 0x03 && arg) {
        i32 *pos = (i32*)arg;
        mouse_state.x = pos[0];
        mouse_state.y = pos[1];
        return 0;
    }
    
    return -1;
}

inode_ops virtio_mouse_ops = {
    .read = virtio_mouse_fs_read,
    .ioctl = virtio_mouse_ioctl
};