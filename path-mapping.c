#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <dlfcn.h> // dlsym
#include <fcntl.h> // stat
#include <dirent.h> // DIR*
#include <stdarg.h> // va_start, va_arg
#include <sys/vfs.h> // statfs
#include <sys/statvfs.h> // statvfs

//#define DEBUG
//#define QUIET

#ifdef QUIET
    #define info_fprintf(...) // remove all info_fprintf including args
#else
    #define info_fprintf fprintf // compile info_fprintf as fprintf
#endif

#ifdef DEBUG
    #define debug_fprintf fprintf // compile debug_fprintf as fprintf
#else
    #define debug_fprintf(...) // remove all debug_fprintf including args
#endif

#define error_fprintf fprintf // always print errors

// Enable or disable specific overrides (always includes the 64 version if applicable)
// #define DISABLE_OPEN
// #define DISABLE_OPENAT
// #define DISABLE_FOPEN
// #define DISABLE_STAT
// #define DISABLE_FSTATAT
// #define DISABLE_STATFS
// #define DISABLE_XSTAT
// #define DISABLE_ACCESS
// #define DISABLE_OPENDIR
// #define DISABLE_READLINK

// List of path pairs. Paths beginning with the first item will be
// translated by replacing the matching part with the second item.
static const char *path_map[][2] = {
    { "/etc/ownCloud", "/home/user1/.etc/ownCloud" },
    { "/tmp/path-mapping/tests/virtual", "/tmp/path-mapping/tests/real" },
};

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif


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
                error_fprintf(stderr, "ERROR fix_path: Path too long: %s", path);
                return path;
            }
            const char *rest = path + prefix_length;
            strcpy(new_path, replace);
            strcat(new_path, rest);
            info_fprintf(stderr, "Mapped Path: '%s' => '%s'\n", path, new_path);
            return new_path;
        }
    }
    return path;
}


#ifndef DISABLE_OPEN
typedef int (*orig_open_func_type)(const char *pathname, int flags, ...);

int open(const char *pathname, int flags, ...)
{
    debug_fprintf(stderr, "open(%s) called\n", pathname);

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
    debug_fprintf(stderr, "open64(%s) called\n", pathname);

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
#endif // DISABLE_OPEN


#ifndef DISABLE_OPENAT
typedef int (*orig_openat_func_type)(int dirfd, const char *pathname, int flags, ...);

int openat(int dirfd, const char *pathname, int flags, ...)
{
    debug_fprintf(stderr, "openat(%s) called\n", pathname);

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
    debug_fprintf(stderr, "openat64(%s) called\n", pathname);

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
#endif // DISABLE_OPENAT


#ifndef DISABLE_FOPEN
typedef FILE* (*orig_fopen_func_type)(const char *path, const char *mode);

FILE * fopen ( const char * filename, const char * mode )
{
    debug_fprintf(stderr, "fopen(%s) called\n", filename);

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
    debug_fprintf(stderr, "fopen64(%s) called\n", filename);

    char buffer[MAX_PATH];
    const char *new_path = fix_path(filename, buffer, sizeof buffer);

    static orig_fopen_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_fopen_func_type)dlsym(RTLD_NEXT, "fopen64");
    }

    return orig_func(new_path, mode);
}
#endif // DISABLE_FOPEN


#ifndef DISABLE_STAT
typedef int (*orig_stat_func_type)(const char *path, struct stat *buf);

int stat(const char *path, struct stat *buf)
{
    debug_fprintf(stderr, "stat(%s) called\n", path);

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
    debug_fprintf(stderr, "lstat(%s) called\n", path);

    char buffer[MAX_PATH];
    const char *new_path = fix_path(path, buffer, sizeof buffer);

    static orig_stat_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_stat_func_type)dlsym(RTLD_NEXT, "lstat");
    }

    return orig_func(new_path, buf);
}
#endif // DISABLE_STAT


#ifndef DISABLE_FSTATAT
typedef int (*orig_fstatat_func_type)(int dirfd, const char *pathname, struct stat *statbuf, int flags);
typedef int (*orig_fstatat64_func_type)(int dirfd, const char *pathname, struct stat64 *statbuf, int flags);
typedef int (*orig_fxstatat_func_type)(int ver, int dirfd, const char *pathname, struct stat *statbuf, int flags);
typedef int (*orig_fxstatat64_func_type)(int ver, int dirfd, const char *pathname, struct stat64 *statbuf, int flags);

int fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags)
{
    debug_fprintf(stderr, "fstatat(%s) called\n", pathname);

    char buffer[MAX_PATH];
    const char *new_path = fix_path(pathname, buffer, sizeof buffer);

    static orig_fstatat_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_fstatat_func_type)dlsym(RTLD_NEXT, "fstatat");
    }

    return orig_func(dirfd, new_path, statbuf, flags);
}

int fstatat64(int dirfd, const char *pathname, struct stat64 *statbuf, int flags)
{
    debug_fprintf(stderr, "fstatat64(%s) called\n", pathname);

    char buffer[MAX_PATH];
    const char *new_path = fix_path(pathname, buffer, sizeof buffer);

    static orig_fstatat64_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_fstatat64_func_type)dlsym(RTLD_NEXT, "fstatat64");
    }

    return orig_func(dirfd, new_path, statbuf, flags);
}

int __fxstatat(int ver, int dirfd, const char *pathname, struct stat *statbuf, int flags)
{
    debug_fprintf(stderr, "__fxstatat(%s) called\n", pathname);

    char buffer[MAX_PATH];
    const char *new_path = fix_path(pathname, buffer, sizeof buffer);

    static orig_fxstatat_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_fxstatat_func_type)dlsym(RTLD_NEXT, "__fxstatat");
    }

    return orig_func(ver, dirfd, new_path, statbuf, flags);
}

int __fxstatat64(int ver, int dirfd, const char *pathname, struct stat64 *statbuf, int flags)
{
    debug_fprintf(stderr, "__fxstatat64(%s) called\n", pathname);

    char buffer[MAX_PATH];
    const char *new_path = fix_path(pathname, buffer, sizeof buffer);

    static orig_fxstatat64_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_fxstatat64_func_type)dlsym(RTLD_NEXT, "__fxstatat64");
    }

    return orig_func(ver, dirfd, new_path, statbuf, flags);
}
#endif // DISABLE_FSTATAT


#ifndef DISABLE_STATFS
typedef int (*orig_statfs_func_type)(const char *path, struct statfs *buf);
typedef int (*orig_statvfs_func_type)(const char *path, struct statvfs *buf);

int statfs(const char *path, struct statfs *buf)
{
    debug_fprintf(stderr, "statfs(%s) called\n", path);

    char buffer[MAX_PATH];
    const char *new_path = fix_path(path, buffer, sizeof buffer);

    static orig_statfs_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_statfs_func_type)dlsym(RTLD_NEXT, "statfs");
    }

    return orig_func(new_path, buf);
}

int statvfs(const char *path, struct statvfs *buf)
{
    debug_fprintf(stderr, "statvfs(%s) called\n", path);

    char buffer[MAX_PATH];
    const char *new_path = fix_path(path, buffer, sizeof buffer);

    static orig_statvfs_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_statvfs_func_type)dlsym(RTLD_NEXT, "statvfs");
    }

    return orig_func(new_path, buf);
}
#endif // DISABLE_STATFS


#ifndef DISABLE_XSTAT
typedef int (*orig_xstat64_func_type)(int ver, const char * path, struct stat64 * stat_buf);

int __xstat64(int ver, const char * path, struct stat64 * stat_buf)
{
    debug_fprintf(stderr, "__xstat64(%s) called\n", path);

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
    debug_fprintf(stderr, "__lxstat64(%s) called\n", path);

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
    debug_fprintf(stderr, "__xstat(%s) called\n", path);

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
    debug_fprintf(stderr, "__lxstat(%s) called\n", path);

    char buffer[MAX_PATH];
    const char *new_path = fix_path(path, buffer, sizeof buffer);

    static orig_xstat_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_xstat_func_type)dlsym(RTLD_NEXT, "__lxstat");
    }

    return orig_func(ver, new_path, stat_buf);
}
#endif // DISABLE_XSTAT


#ifndef DISABLE_ACCESS
typedef int (*orig_access_func_type)(const char *pathname, int mode);

int access(const char *pathname, int mode)
{
    debug_fprintf(stderr, "access(%s) called\n", pathname);

    char buffer[MAX_PATH];
    const char *new_path = fix_path(pathname, buffer, sizeof buffer);

    orig_access_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_access_func_type)dlsym(RTLD_NEXT, "access");
    }

    return orig_func(new_path, mode);
}
#endif // DISABLE_ACCESS


#ifndef DISABLE_OPENDIR
typedef DIR* (*orig_opendir_func_type)(const char *name);

DIR *opendir(const char *name)
{
    debug_fprintf(stderr, "opendir(%s) called\n", name);

    char buffer[MAX_PATH];
    const char *new_path = fix_path(name, buffer, sizeof buffer);

    static orig_opendir_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_opendir_func_type)dlsym(RTLD_NEXT, "opendir");
    }

    return orig_func(new_path);
}
#endif // DISABLE_OPENDIR


#ifndef DISABLE_READLINK
typedef ssize_t (*orig_readlink_func_type)(const char *pathname, char *buf, size_t bufsiz);
typedef ssize_t (*orig_readlinkat_func_type)(int dirfd, const char *pathname, char *buf, size_t bufsiz);

ssize_t readlink(const char *pathname, char *buf, size_t bufsiz)
{
    debug_fprintf(stderr, "readlink(%s, buf, %d) called\n", pathname, bufsiz);

    char buffer[MAX_PATH];
    const char *new_path = fix_path(pathname, buffer, sizeof buffer);

    static orig_readlink_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_readlink_func_type)dlsym(RTLD_NEXT, "readlink");
    }

    // The returned content of the link is not mapped in reverse. It could also be relative.
    return orig_func(new_path, buf, bufsiz);
}

ssize_t readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz)
{
    debug_fprintf(stderr, "readlinkat(%s, buf, %d) called\n", pathname, bufsiz);

    char buffer[MAX_PATH];
    const char *new_path = fix_path(pathname, buffer, sizeof buffer);

    static orig_readlinkat_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_readlinkat_func_type)dlsym(RTLD_NEXT, "readlinkat");
    }

    // The returned content of the link is not mapped in reverse. It could also be relative.
    return orig_func(dirfd, new_path, buf, bufsiz);
}
#endif // DISABLE_READLINK
