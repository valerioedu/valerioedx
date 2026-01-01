int main() {
    /*const char *msg = "Hello, Userland!\n";
    asm volatile(
        "mov x8, #4\n"
        "mov x0, #1\n"
        "mov x1, %x[buf]\n"
        "mov x2, #17\n"
        "svc #0\n"
        :
        : [buf] "r" (msg)
        : "x0","x1","x2","x8","memory"
    );*/

    const char *txt = "/TEST.TXT";
    asm volatile(
        "mov x8, #5\n"
        "mov x0, %x[bf]\n"
        "mov x1, #0\n"
        "svc #0"
        :: [bf] "r" (txt)
        : "x0", "x1", "x8", "memory"
    );

    char in[16] = { 0 };
    asm volatile(
        "mov x8, #3\n"
        "mov x0, #4\n"
        "mov x1, %x[buf]\n"
        "mov x2, #16\n"
        "svc #0\n"
        :
        : [buf] "r" (in)
        : "x0","x1","x2","x8","memory"
    );

    asm volatile(
        "mov x8, #4\n"
        "mov x0, #1\n"
        "mov x1, %x[buf]\n"
        "mov x2, #16\n"
        "svc #0\n"
        :: [buf] "r" (in)
        : "x0", "x1", "x2", "x8", "memory"
    );

    while (1) asm volatile("wfi");
    return 0;
}