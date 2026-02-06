#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64

// Built-in commands
extern int cd(char *str);
extern int pwd();

const char* get_signal_name(int sig) {
    switch (sig) {
        case SIGHUP:  return "Hangup";
        case SIGINT:  return "Interrupt";
        case SIGQUIT: return "Quit";
        case SIGILL:  return "Illegal instruction";
        case SIGTRAP: return "Trace/breakpoint trap";
        case SIGABRT: return "Aborted";
        case SIGBUS:  return "Bus error";
        case SIGFPE:  return "Floating point exception";
        case SIGKILL: return "Killed";
        case SIGSEGV: return "Segmentation fault";
        case SIGPIPE: return "Broken pipe";
        case SIGALRM: return "Alarm clock";
        case SIGTERM: return "Terminated";
        default:      return NULL;
    }
}

int run_command(const char *cmd, char *argv[]) {
    pid_t pid = fork();

    if (pid == 0) {
        char *envp[] = {NULL};
        char path[256];
        snprintf(path, sizeof(path), "/bin/%s", cmd);
        execve(path, argv, envp);
        snprintf(path, sizeof(path), "/sbin/%s", cmd);
        execve(path, argv, envp);
        snprintf(path, sizeof(path), "/usr/bin/%s", cmd);
        execve(path, argv, envp);
        snprintf(path, sizeof(path), "/usr/sbin/%s", cmd);
        execve(path, argv, envp);
        printf("command not found: %s\n", argv[0]);
        _exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        
        int sig = 0;

        if (WIFSIGNALED(status)) {
            sig = WTERMSIG(status);
        } 
        else if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            if (code > 128) {
                sig = code - 128;
            }
        }

        if (sig > 0) {
            const char *name = get_signal_name(sig);
            if (name) printf("%s\n", name);
            else printf("Terminated by signal %d\n", sig);
        }

        return status;
    } else {
        printf("fork failed\n");
        return -1;
    }
}

int main() {
    char buf[MAX_CMD_LEN];
    char dir[128];
    char *argv[MAX_ARGS];
    int argc;

    while (1) {
        if (getcwd(dir, sizeof(dir)) != NULL)

        else printf("valerioedx:?$ ");
        fflush(stdout);

        if (fgets(buf, sizeof(buf), stdin) == NULL) break;
        
        buf[strcspn(buf, "\n")] = '\0';
        
        // Skip empty commands
        if (buf[0] == '\0') continue;

        argc = 0;
        char *saveptr;
        char *token = strtok_r(buf, " \t", &saveptr);
        while (token != NULL && argc < MAX_ARGS - 1) {
            argv[argc++] = token;
            token = strtok_r(NULL, " \t", &saveptr);
        }

        argv[argc] = NULL;

        if (argc == 0) continue;
        if (strcmp(argv[0], "exit") == 0)
            return 0;

        else if (strcmp(argv[0], "cd") == 0)
            cd(argv[1]);

        else if (strcmp(argv[0], "pwd") == 0)
            pwd();

        else if (strcmp(argv[0], "export") == 0) {
            if (argv[1]) {
                char *name = argv[1];
                char *value = strchr(name, '=');
                
                if (value) {
                    *value = 0; // Split the string at '='
                    value++;    // Move pointer to the value part
                    if (setenv(name, value, 1) != 0) {
                        printf("export: failed to set variable\n");
                    }
                } else {
                    // Case: export VAR (without value) - typically ignores or sets empty
                    // For now, let's warn
                    printf("export: usage export NAME=VALUE\n");
                }
            } else {
                // Case: export (with no args) - usually prints env
                // We can call our printenv utility logic here or just ignore
                extern char **environ;
                for (char **env = environ; *env; ++env) {
                    printf("%s\n", *env);
                }
            }
        }

        else 
            run_command(argv[0], argv);
    }

    return 0;
}

void _start() {
    asm volatile(
        "bl main\n"
        "mov x8, #1\n"
        "svc #0\n"              // _exit(return value in x0)
    );
}