int reboot(int howto) {
    register int x0 asm("x0") = howto;
    register int x8 asm("x8") = 55;
    asm volatile("svc #0"
                    : "+r"(x0) : "r"(x8) : "memory");
    return x0;
}