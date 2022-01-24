#define _POSIX_C_SOURCE 200112L // setenv
#include <stdlib.h> // clearenv
#include <unistd.h> // execl
#include <stdio.h> // printf
#include <string.h> // strcmp


extern char **environ;

int main(int argc, char **argv) {
    if (argc != 4) {
        printf("Usage: $0 [calltype] [executable path] [nargs 0...3]\n");
        return 1;
    }
    const char *calltype = argv[1];
    const char *executable = argv[2];
    const char nargs = argv[3][0];

    environ[0] = NULL;
    setenv("TEST0", "value0", 1);

    if (strcmp(calltype, "execl") == 0) {
        switch (nargs) {
            case '0': execl(executable, executable, NULL); // no break because exec replaces the current process
            case '1': execl(executable, executable, "arg1", NULL);
            case '2': execl(executable, executable, "arg1", "arg2", NULL);
            case '3': execl(executable, executable, "arg1", "arg2", "arg3", NULL);
        }
    }
    if (strcmp(calltype, "execlp") == 0) {
        switch (nargs) {
            case '0': execlp(executable, executable, NULL); // no break because exec replaces the current process
            case '1': execlp(executable, executable, "arg1", NULL);
            case '2': execlp(executable, executable, "arg1", "arg2", NULL);
            case '3': execlp(executable, executable, "arg1", "arg2", "arg3", NULL);
        }
    }
    if (strcmp(calltype, "execle") == 0) {
        const char *env[] = {
            "TEST1=value1",
            "TEST2=value2",
            NULL,
        };
        switch (nargs) {
            case '0': execle(executable, executable, NULL, env); // no break because exec replaces the current process
            case '1': execle(executable, executable, "arg1", NULL, env);
            case '2': execle(executable, executable, "arg1", "arg2", NULL, env);
            case '3': execle(executable, executable, "arg1", "arg2", "arg3", NULL, env);
        }
    }
    return 0;
}