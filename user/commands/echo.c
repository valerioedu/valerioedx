#include <unistd.h>
#include <string.h>

int main(char *argv) {
    int len = strlen(argv);
    write(1, argv, len);
    return 0;
}