#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdarg.h>
#include <malloc.h>

// List of path pairs. Paths beginning with the first item will be
// translated by replacing the matching part with the second item.
static const char *path_map[][2] = {
    { "/etc/ownCloud/", "/home/user1/.etc/ownCloud/" },
};

__thread char *buffer = NULL;
__thread int buffer_size = -1;

typedef FILE* (*orig_fopen_func_type)(const char *path, const char *mode);
typedef int (*orig_open_func_type)(const char *pathname, int flags, ...);
typedef int (*orig_openat_func_type)(int dirfd, const char *pathname, int flags, ...);
typedef int (*orig_stat_func_type)(const char *path, struct stat *buf);
typedef DIR* (*orig_opendir_func_type)(const char *name);

static int starts_with(const char *str, const char *prefix) {
    return (strncmp(prefix, str, strlen(prefix)) == 0);
}

static char *get_buffer(int min_size) {
    int step = 63;
    if (min_size < 1) {
        min_size = 1;
    }
    if (min_size > buffer_size) {
        if (buffer != NULL) {
            free(buffer);
            buffer = NULL;
            buffer_size = -1;
        }
        buffer = malloc(min_size + step);
        if (buffer != NULL) {
            buffer_size = min_size + step;
        }
    }
    return buffer;
}

static const char *fix_path(const char *path)
{
    int count = (sizeof path_map) / (sizeof *path_map); // Array length
    for (int i = 0; i < count; i++) {
        const char *prefix = path_map[i][0];
        const char *replace = path_map[i][1];
        if (starts_with(path, prefix)) {
            const char *rest = path + strlen(prefix);
            char *new_path = get_buffer(strlen(path) + strlen(replace) - strlen(prefix));
            strcpy(new_path, replace);
            strcat(new_path, rest);
            printf("Mapped Path: %s  ==>  %s\n", path, new_path);
            return new_path;
        }
    }
    return path;
}


int open(const char *pathname, int flags, ...)
{
#ifdef DEBUG
    printf("open(%s) called\n", pathname);
#endif

    const char *new_path = fix_path(pathname);

    orig_open_func_type orig_func;
    orig_func = (orig_open_func_type)dlsym(RTLD_NEXT, "open");

    // If O_CREAT is used to create a file, the file access mode must be given.
    if (__OPEN_NEEDS_MODE(flags)) {
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
#ifdef DEBUG
    printf64("open64(%s) called\n", pathname);
#endif

    const char *new_path = fix_path(pathname);

    orig_open_func_type orig_func;
    orig_func = (orig_open_func_type)dlsym(RTLD_NEXT, "open64");

    // If O_CREAT is used to create a file, the file access mode must be given.
    if (__OPEN_NEEDS_MODE(flags)) {
        va_list args;
        va_start(args, flags);
        int mode = va_arg(args, int);
        va_end(args);
        return orig_func(new_path, flags, mode);
    } else {
        return orig_func(new_path, flags);
    }
}

int openat(int dirfd, const char *pathname, int flags, ...)
{
#ifdef DEBUG
    printf("openat(%s) called\n", pathname);
#endif

    const char *new_path = fix_path(pathname);

    orig_openat_func_type orig_func;
    orig_func = (orig_openat_func_type)dlsym(RTLD_NEXT, "openat");

    // If O_CREAT is used to create a file, the file access mode must be given.
    if (__OPEN_NEEDS_MODE(flags)) {
        va_list args;
        va_start(args, flags);
        int mode = va_arg(args, int);
        va_end(args);
        return orig_func(dirfd, new_path, flags, mode);
    } else {
        return orig_func(dirfd, new_path, flags);
    }
}

int openat64(int dirfd, const char *pathname, int flags, ...)
{
#ifdef DEBUG
    printf("openat64(%s) called\n", pathname);
#endif

    const char *new_path = fix_path(pathname);

    orig_openat_func_type orig_func;
    orig_func = (orig_openat_func_type)dlsym(RTLD_NEXT, "openat64");

    // If O_CREAT is used to create a file, the file access mode must be given.
    if (__OPEN_NEEDS_MODE(flags)) {
        va_list args;
        va_start(args, flags);
        int mode = va_arg(args, int);
        va_end(args);
        return orig_func(dirfd, new_path, flags, mode);
    } else {
        return orig_func(dirfd, new_path, flags);
    }
}

FILE * fopen ( const char * filename, const char * mode )
{
#ifdef DEBUG
    printf("fopen(%s) called\n", filename);
#endif

    const char *new_path = fix_path(filename);

    orig_fopen_func_type orig_func;
    orig_func = (orig_fopen_func_type)dlsym(RTLD_NEXT, "fopen");

    return orig_func(new_path, mode);
}

FILE * fopen64 ( const char * filename, const char * mode )
{
#ifdef DEBUG
    printf("fopen64(%s) called\n", filename);
#endif

    const char *new_path = fix_path(filename);

    orig_fopen_func_type orig_func;
    orig_func = (orig_fopen_func_type)dlsym(RTLD_NEXT, "fopen64");

    return orig_func(new_path, mode);
}

int stat(const char *path, struct stat *buf)
{
#ifdef DEBUG
    printf("stat(%s) called\n", path);
#endif

    const char *new_path = fix_path(path);

    orig_stat_func_type orig_func;
    orig_func = (orig_stat_func_type)dlsym(RTLD_NEXT, "stat");

    return orig_func(new_path, buf);
}

int lstat(const char *path, struct stat *buf)
{
#ifdef DEBUG
    printf("lstat(%s) called\n", path);
#endif

    const char *new_path = fix_path(path);

    orig_stat_func_type orig_func;
    orig_func = (orig_stat_func_type)dlsym(RTLD_NEXT, "lstat");

    return orig_func(new_path, buf);
}

DIR *opendir(const char *name)
{
#ifdef DEBUG
    printf("opendir(%s) called\n", name);
#endif

    const char *new_path = fix_path(name);

    orig_opendir_func_type orig_func;
    orig_func = (orig_opendir_func_type)dlsym(RTLD_NEXT, "opendir");

    return orig_func(new_path);
}
