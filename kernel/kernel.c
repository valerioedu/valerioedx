void main(void) {
    volatile unsigned short* vga = (unsigned short*)0xB8000;
    const char* msg = "Entered 64-bit kernel!";
    unsigned char attr = 0x2F; // white on green

    for (int i = 0; msg[i] != '\0'; ++i) {
        vga[i] = (unsigned short)msg[i] | ((unsigned short)attr << 8);
    }

    while (1) __asm__ volatile("hlt");
}