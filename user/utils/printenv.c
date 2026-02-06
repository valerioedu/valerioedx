#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    char **env = environ;
    
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            char *val = getenv(argv[i]);
            if (val) printf("%s\n", val);
        }
        return 0;
    }

    while (*env) {
        printf("%s\n", *env);
        env++;
    }

    return 0;
}