#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("rmdir: missing operand\n");
        return -1;
    }
    
    int ret = rmdir(argv[1]);
    if (ret < 0) {
        printf("rmdir: failed to remove '%s'\n", argv[1]);
        return -1;
    }
    
    return 0;
}