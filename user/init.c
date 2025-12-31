int main() {
    const char *msg = "Hello, Userland!\n";
    asm volatile(
        "mov x8, #4\n"
        "mov x0, #1\n"
        "mov x1, %x[buf]\n"
        "mov x2, #17\n"
        "svc #0\n"
        :
        : [buf] "r" (msg)
        : "x0","x1","x2","x8","memory"
    );

    while (1) asm volatile("wfi");
    return 0;
}