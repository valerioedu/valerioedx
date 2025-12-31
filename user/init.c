int main() {
        const char *msg = "valerioedx:\n";
        asm volatile(
            "mov x8, #4\n"
            "mov x0, #1\n"
            "mov x1, %x[buf]\n"
            "mov x2, #12\n"
            "svc #0\n"
            :
            : [buf] "r" (msg)
            : "x0","x1","x2","x8","memory"
        );
        char input[33] = {0};

        asm volatile(
            "mov x8, #3\n"
            "mov x0, #0\n"
            "mov x1, %x[in]\n"
            "mov x2, #32\n"
            "svc #0\n"
            :
            : [in] "r" (input)
            : "x0", "x1", "x2", "x8", "memory"
        );

    while (1) asm volatile("wfi");
    return 0;
}