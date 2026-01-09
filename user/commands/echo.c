#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc > 1) {
        int len = strlen(argv[1]);
        write(1, argv[1], len);
    }
    write(1, "Hi\n", 3);
    _exit(0);
    return 0;
}

void _start() {
    // The kernel sets up argc at sp, argv at sp+8
    // For simplicity, call main directly - proper impl would parse stack
    register unsigned long sp_val;
    asm volatile("mov %0, sp" : "=r"(sp_val));
    
    unsigned long argc = *(unsigned long*)sp_val;
    char **argv = (char**)(sp_val + 8);
    
    _exit(main(argc, argv));
}