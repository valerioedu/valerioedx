#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int export(char *arg) {
    if (arg) {
        char *name = arg;
        char *value = strchr(name, '=');
        
        if (value) {
            *value = 0;
            value++;
            if (setenv(name, value, 1) != 0)
                printf("export: failed to set variable\n");

        } else {
            printf("export: usage export NAME=VALUE\n");
            return 1;
        }
    } else {
        extern char **environ;
        for (char **env = environ; *env; ++env)
            printf("%s\n", *env);
    }

    return 0;
}