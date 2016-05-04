#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <alloca.h>

#define PREFIX "/etc/ownCloud/"
#define NEW_PREFIX "/home/webering/oc-dev/etc/ownCloud/"

typedef FILE* (*orig_fopen_func_type)(const char *path, const char *mode);
typedef int (*orig_open_func_type)(const char *pathname, int flags, ...);

/*int log_file_access(const char *path) {
    if (strncmp(path, PREFIX, (sizeof PREFIX) - 1) != 0) {
        // Not a file we are interested in
        return -1;
    }

    // Log file access to a file. Need to use flock or a similar locking
    // approach if all the accesses are written to the same file.
    // ...
    printf("Opened file %s\n", path);
}

const char *fix_path(const char *path, char *new_path)
{
    if (strncmp(path, PREFIX, (sizeof PREFIX) - 1) == 0) {
        strcpy(new_path, NEW_PREFIX);
        strcat(new_path, path + (sizeof PREFIX) - 1);
        printf("New Path: %s\n", new_path);
        return new_path;
    }
    else {
        printf("Old Path: %s\n", path);
        return path;
    }
}*/

FILE* fopen(const char *path, const char *mode)
{
    // Note: alloca() memory gets freed automatically at the end of THIS function
    //const char *new_path = fix_path(path, alloca(strlen(path) + (sizeof NEW_PREFIX)));
    const char *new_path = path;

    orig_fopen_func_type orig_func;
    orig_func = (orig_fopen_func_type)dlsym(RTLD_NEXT, "fopen");
    return orig_func(new_path, mode);
}

FILE* fopen64(const char *path, const char *mode)
{
    // Note: alloca() memory gets freed automatically at the end of THIS function
    //const char *new_path = fix_path(path, alloca(strlen(path) + (sizeof NEW_PREFIX)));
    const char *new_path = path;

    orig_fopen_func_type orig_func;
    orig_func = (orig_fopen_func_type)dlsym(RTLD_NEXT, "fopen64");
    return orig_func(new_path, mode);
}

int open(const char *pathname, int flags, ...)
{
    // Note: alloca() memory gets freed automatically at the end of THIS function
    //const char *new_path = fix_path(pathname, alloca(strlen(pathname) + (sizeof NEW_PREFIX)));
    const char *new_path = pathname;

    orig_open_func_type orig_func;
    orig_func = (orig_open_func_type)dlsym(RTLD_NEXT, "open");

    // If O_CREAT is used to create a file, the file access mode must be given.
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        int mode = va_arg(args, int);
        va_end(args);
        return orig_func(new_path, flags, mode);
    } else {
        return orig_func(new_path, flags);
    }
}

int open64(const char *pathname, int flags, ...)
{
    // Note: alloca() memory gets freed automatically at the end of THIS function
    //const char *new_path = fix_path(pathname, alloca(strlen(pathname) + (sizeof NEW_PREFIX)));
    const char *new_path = pathname;

    orig_open_func_type orig_func;
    orig_func = (orig_open_func_type)dlsym(RTLD_NEXT, "open64");

    // If O_CREAT is used to create a file, the file access mode must be given.
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        int mode = va_arg(args, int);
        va_end(args);
        return orig_func(new_path, flags, mode);
    } else {
        return orig_func(new_path, flags);
    }
}

