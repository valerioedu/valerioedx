#include <uart.h>
#include <vfs.h>

u64 uart_read_fs(inode_t* node, u64 size, u8* buffer) {
    //TODO: Implement uart_getc
    return 0; 
}

u64 uart_write_fs(inode_t* node, u64 size, u8* buffer) {
    char* str = (char*)buffer;
    for (u64 i = 0; i < size; i++) {
        uart_putc(str[i]);
    }
    return size;
}