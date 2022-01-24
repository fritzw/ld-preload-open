#include <unistd.h>
#include <stdio.h>

extern char **environ;

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        printf("%s\n", argv[i]);
    }

    for (char **env = environ; *env != NULL; env++)
    {
        char *thisEnv = *env;
        printf("%s\n", thisEnv);    
    }
    return 0;
}