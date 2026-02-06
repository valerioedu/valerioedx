#include <unistd.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("rm: missing operand\n");
        return 1;
    }

    int ret = unlink(argv[1]);
    if (ret < 0) {
        printf("rm: failed to remove '%s'\n", argv[1]);
        return 1;
    }

    return 0;
}