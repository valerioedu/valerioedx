#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>

// Matches the internal structure from virtio-mouse.c
typedef struct {
    int32_t rel_x;
    int32_t rel_y;
    uint8_t buttons;
} mouse_event_t;

int main() {
    // According to devfs.c, the mouse is mounted as "virtio-mouse"
    int fd = open("/dev/virtio-mouse", O_RDONLY);
    if (fd < 0) {
        printf("Error: Could not open /dev/virtio-mouse\n");
        return 1;
    }

    printf("Mouse tracker started. Move the mouse!\n");

    mouse_event_t event;
    int32_t abs_pos[2];

    while (1) {
        // This read will block until the wait_queue is woken up by virtio_mouse_push_event
        ssize_t bytes_read = read(fd, &event, sizeof(mouse_event_t));
        
        if (bytes_read == sizeof(mouse_event_t)) {
            // Optional: You can also use IOCTL 0x01 to get the absolute position
            // tracked by the kernel.
            if (ioctl(fd, 0x01, (uint64_t)abs_pos) == 0) {
                printf("Abs(X:%d, Y:%d) | ", abs_pos[0], abs_pos[1]);
            }

            printf("Rel(X:%d, Y:%d) | Buttons: ", event.rel_x, event.rel_y);
            
            // Check button bitmask (bit0=left, bit1=right, bit2=middle)
            if (event.buttons == 0) {
                printf("None");
            } else {
                if (event.buttons & 0x01) printf("[Left] ");
                if (event.buttons & 0x02) printf("[Right] ");
                if (event.buttons & 0x04) printf("[Middle] ");
            }
            printf("\n");
            
        } else {
            printf("Error reading from mouse device.\n");
            break;
        }
    }

    close(fd);
    return 0;
}