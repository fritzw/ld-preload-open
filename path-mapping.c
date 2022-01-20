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

//#define DEBUG

// Enable or disable specific overrides
#define ENABLE_XSTAT

// List of path pairs. Paths beginning with the first item will be
// translated by replacing the matching part with the second item.
static const char *path_map[][2] = {
    { "/etc/ownCloud", "/home/user1/.etc/ownCloud" },
};

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

typedef FILE* (*orig_fopen_func_type)(const char *path, const char *mode);
typedef int (*orig_open_func_type)(const char *pathname, int flags, ...);
typedef int (*orig_openat_func_type)(int dirfd, const char *pathname, int flags, ...);
typedef int (*orig_stat_func_type)(const char *path, struct stat *buf);
typedef int (*orig_access_func_type)(const char *pathname, int mode);
typedef DIR* (*orig_opendir_func_type)(const char *name);

// Returns strlen(path) without trailing slashes
size_t pathlen(const char *path)
{
    size_t path_length = strlen(path);
    while (path_length > 0 && path[path_length - 1] == '/') {
        // If the prefix ends with a slash ("/example/dir/"), ignore the slash.
        // Otherwise it would not match the dir itself ("/examle/dir"), e.g. in opendir().
        path_length -= 1;
    }
    return path_length;
}

// Returns true if the first path components of path match those of prefix (whole word matches only)
int path_prefix_matches(const char *prefix, const char *path)
{
    size_t prefix_len = pathlen(prefix);
    if (strncmp(prefix, path, prefix_len) == 0) {
        // The prefix matches, but "/example/dir" would also match "/example/dirty/file"
        // Thus we only return true if a slash or end-of-string follows the match.
        char char_after_match = path[prefix_len];
        return char_after_match == '/' || char_after_match == '\0';
    }
    return 0;
}

// Check if path matches any defined prefix, and if so, replace it with its substitution
static const char *fix_path(const char *path, char *new_path, size_t new_path_size)
{
    int count = (sizeof path_map) / (sizeof *path_map); // Array length
    for (int i = 0; i < count; i++) {
        const char *prefix = path_map[i][0];
        if (path_prefix_matches(prefix, path)) {
            const char *replace = path_map[i][1];
            size_t prefix_length = pathlen(prefix);
            size_t new_length = strlen(path) + pathlen(replace) - prefix_length;
            if (new_length > new_path_size - 1) {
                fprintf(stderr, "path-mapping: Path too long: %s", path);
                return path;
            }
            const char *rest = path + prefix_length;
            strcpy(new_path, replace);
            strcat(new_path, rest);
            printf("Mapped Path: '%s' => '%s'\n", path, new_path);
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

    char buffer[MAX_PATH];
    const char *new_path = fix_path(pathname, buffer, sizeof buffer);

    static orig_open_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_open_func_type)dlsym(RTLD_NEXT, "open");
    }

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
    printf("open64(%s) called\n", pathname);
#endif

    char buffer[MAX_PATH];
    const char *new_path = fix_path(pathname, buffer, sizeof buffer);

    static orig_open_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_open_func_type)dlsym(RTLD_NEXT, "open64");
    }

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

    char buffer[MAX_PATH];
    const char *new_path = fix_path(pathname, buffer, sizeof buffer);

    static orig_openat_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_openat_func_type)dlsym(RTLD_NEXT, "openat");
    }

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

    char buffer[MAX_PATH];
    const char *new_path = fix_path(pathname, buffer, sizeof buffer);

    static orig_openat_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_openat_func_type)dlsym(RTLD_NEXT, "openat64");
    }

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

    char buffer[MAX_PATH];
    const char *new_path = fix_path(filename, buffer, sizeof buffer);

    static orig_fopen_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_fopen_func_type)dlsym(RTLD_NEXT, "fopen");
    }

    return orig_func(new_path, mode);
}

FILE * fopen64 ( const char * filename, const char * mode )
{
#ifdef DEBUG
    printf("fopen64(%s) called\n", filename);
#endif

    char buffer[MAX_PATH];
    const char *new_path = fix_path(filename, buffer, sizeof buffer);

    static orig_fopen_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_fopen_func_type)dlsym(RTLD_NEXT, "fopen64");
    }

    return orig_func(new_path, mode);
}

int stat(const char *path, struct stat *buf)
{
#ifdef DEBUG
    printf("stat(%s) called\n", path);
#endif

    char buffer[MAX_PATH];
    const char *new_path = fix_path(path, buffer, sizeof buffer);

    static orig_stat_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_stat_func_type)dlsym(RTLD_NEXT, "stat");
    }

    return orig_func(new_path, buf);
}

int lstat(const char *path, struct stat *buf)
{
#ifdef DEBUG
    printf("lstat(%s) called\n", path);
#endif

    char buffer[MAX_PATH];
    const char *new_path = fix_path(path, buffer, sizeof buffer);

    static orig_stat_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_stat_func_type)dlsym(RTLD_NEXT, "lstat");
    }

    return orig_func(new_path, buf);
}

#ifdef ENABLE_XSTAT
typedef int (*orig_xstat64_func_type)(int ver, const char * path, struct stat64 * stat_buf);
int __xstat64(int ver, const char * path, struct stat64 * stat_buf)
{
#ifdef DEBUG
    printf("__xstat64(%s) called\n", path);
#endif

    char buffer[MAX_PATH];
    const char *new_path = fix_path(path, buffer, sizeof buffer);

    static orig_xstat64_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_xstat64_func_type)dlsym(RTLD_NEXT, "__xstat64");
    }

    return orig_func(ver, new_path, stat_buf);
}

int __lxstat64(int ver, const char * path, struct stat64 * stat_buf)
{
#ifdef DEBUG
    printf("__lxstat64(%s) called\n", path);
#endif

    char buffer[MAX_PATH];
    const char *new_path = fix_path(path, buffer, sizeof buffer);

    static orig_xstat64_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_xstat64_func_type)dlsym(RTLD_NEXT, "__lxstat64");
    }

    return orig_func(ver, new_path, stat_buf);
}

typedef int (*orig_xstat_func_type)(int ver, const char * path, struct stat * stat_buf);
int __xstat(int ver, const char * path, struct stat * stat_buf)
{
#ifdef DEBUG
    printf("__xstat(%s) called\n", path);
#endif

    char buffer[MAX_PATH];
    const char *new_path = fix_path(path, buffer, sizeof buffer);

    static orig_xstat_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_xstat_func_type)dlsym(RTLD_NEXT, "__xstat");
    }

    return orig_func(ver, new_path, stat_buf);
}

int __lxstat(int ver, const char * path, struct stat * stat_buf)
{
#ifdef DEBUG
    printf("__lxstat(%s) called\n", path);
#endif

    char buffer[MAX_PATH];
    const char *new_path = fix_path(path, buffer, sizeof buffer);

    static orig_xstat_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_xstat_func_type)dlsym(RTLD_NEXT, "__lxstat");
    }

    return orig_func(ver, new_path, stat_buf);
}
#endif

int access(const char *pathname, int mode)
{
#ifdef DEBUG
    printf("access(%s) called\n", pathname);
#endif

    char buffer[MAX_PATH];
    const char *new_path = fix_path(pathname, buffer, sizeof buffer);

    orig_access_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_access_func_type)dlsym(RTLD_NEXT, "access");
    }

    return orig_func(new_path, mode);
}

DIR *opendir(const char *name)
{
#ifdef DEBUG
    printf("opendir(%s) called\n", name);
#endif

    char buffer[MAX_PATH];
    const char *new_path = fix_path(name, buffer, sizeof buffer);

    static orig_opendir_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_opendir_func_type)dlsym(RTLD_NEXT, "opendir");
    }

    return orig_func(new_path);
}
