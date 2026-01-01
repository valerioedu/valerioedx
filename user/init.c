int main() {
    // Test write
    const char *hello = "Write works!\n";
    asm volatile(
        "mov x8, #4\n"
        "mov x0, #1\n"
        "mov x1, %0\n"
        "mov x2, #13\n"
        "svc #0\n"
        :: "r" (hello)
        : "x0", "x1", "x2", "x8", "memory"
    );

    // Open file - store result directly to memory
    const char *txt = "/TEST.TXT";
    volatile long fd = -1;
    
    asm volatile(
        "mov x8, #5\n"
        "mov x0, %1\n"
        "mov x1, #0\n"
        "svc #0\n"
        "str x0, [%0]\n"
        :: "r" (&fd), "r" (txt)
        : "x0", "x1", "x8", "memory"
    );

    // Print fd value
    char fd_msg[8] = {'f', 'd', '=', '0', '\n', 0, 0, 0};
    if (fd < 0) {
        fd_msg[3] = '-';
    } else {
        fd_msg[3] = '0' + (fd % 10);
    }
    asm volatile(
        "mov x8, #4\n"
        "mov x0, #1\n"
        "mov x1, %0\n"
        "mov x2, #5\n"
        "svc #0\n"
        :: "r" (fd_msg)
        : "x0", "x1", "x2", "x8", "memory"
    );

    if (fd < 0) {
        const char *err = "open failed\n";
        asm volatile(
            "mov x8, #4\n"
            "mov x0, #1\n"
            "mov x1, %0\n"
            "mov x2, #12\n"
            "svc #0\n"
            :: "r" (err)
            : "x0", "x1", "x2", "x8", "memory"
        );
        while(1) asm volatile("wfi");
    }

    // Read from file
    char in[32];
    for (int i = 0; i < 32; i++) in[i] = 'X';
    
    volatile long bytes_read = -1;
    asm volatile(
        "mov x8, #3\n"
        "ldr x0, [%1]\n"
        "mov x1, %2\n"
        "mov x2, #16\n"
        "svc #0\n"
        "str x0, [%0]\n"
        :: "r" (&bytes_read), "r" (&fd), "r" (in)
        : "x0", "x1", "x2", "x8", "memory"
    );

    // Print bytes_read
    char br_msg[16] = {'r', 'e', 'a', 'd', '=', '0', '\n', 0};
    if (bytes_read < 0) {
        br_msg[5] = '-';
    } else {
        br_msg[5] = '0' + (bytes_read % 10);
    }
    asm volatile(
        "mov x8, #4\n"
        "mov x0, #1\n"
        "mov x1, %0\n"
        "mov x2, #7\n"
        "svc #0\n"
        :: "r" (br_msg)
        : "x0", "x1", "x2", "x8", "memory"
    );

    // Write data
    const char *prefix = "data:";
    asm volatile(
        "mov x8, #4\n"
        "mov x0, #1\n"
        "mov x1, %0\n"
        "mov x2, #5\n"
        "svc #0\n"
        :: "r" (prefix)
        : "x0", "x1", "x2", "x8", "memory"
    );

    asm volatile(
        "mov x8, #4\n"
        "mov x0, #1\n"
        "mov x1, %0\n"
        "mov x2, #16\n"
        "svc #0\n"
        :: "r" (in)
        : "x0", "x1", "x2", "x8", "memory"
    );

    const char *done = "\ndone\n";
    asm volatile(
        "mov x8, #4\n"
        "mov x0, #1\n"
        "mov x1, %0\n"
        "mov x2, #6\n"
        "svc #0\n"
        :: "r" (done)
        : "x0", "x1", "x2", "x8", "memory"
    );

    while (1) asm volatile("wfi");
    return 0;
}