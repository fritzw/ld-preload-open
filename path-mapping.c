#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <dlfcn.h> // dlsym
#include <fcntl.h> // stat
#include <dirent.h> // DIR*
#include <stdarg.h> // va_start, va_arg
#include <sys/vfs.h> // statfs
#include <sys/statvfs.h> // statvfs
#include <unistd.h> // uid_t, gid_t
#include <malloc.h> // for execl

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
// #define DISABLE_XATTR
// #define DISABLE_OPENDIR
// #define DISABLE_PATHCONF
// #define DISABLE_REALPATH
// #define DISABLE_READLINK
// #define DISABLE_SYMLINKS
// #define DISABLE_CHMOD
// #define DISABLE_CHOWN
// #define DISABLE_UNLINK
// #define DISABLE_RENAME

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


#include "override-macros.h"

#ifndef DISABLE_OPEN
OVERRIDE_FUNCTION_VARARGS(2, 1, int, open, const char *, pathname, int, flags)
OVERRIDE_FUNCTION_VARARGS(2, 1, int, open64, const char *, pathname, int, flags)
#endif // DISABLE_OPEN


#ifndef DISABLE_OPENAT
OVERRIDE_FUNCTION_VARARGS(3, 2, int, openat, int, dirfd, const char *, pathname, int, flags)
OVERRIDE_FUNCTION_VARARGS(3, 2, int, openat64, int, dirfd, const char *, pathname, int, flags)
#endif // DISABLE_OPENAT


#ifndef DISABLE_FOPEN
OVERRIDE_FUNCTION(2, 1, FILE*, fopen, const char *, filename, const char *, mode)
OVERRIDE_FUNCTION(2, 1, FILE*, fopen64, const char *, filename, const char *, mode)
OVERRIDE_FUNCTION(3, 1, FILE*, freopen, const char *, filename, const char *, mode, FILE *, stream)
#endif // DISABLE_FOPEN


#ifndef DISABLE_STAT
OVERRIDE_FUNCTION(2, 1, int, stat, const char *, path, struct stat *, buf)
OVERRIDE_FUNCTION(2, 1, int, lstat, const char *, path, struct stat *, buf)
#endif // DISABLE_STAT


#ifndef DISABLE_XSTAT
OVERRIDE_FUNCTION(3, 2, int, __xstat, int, ver, const char *, path, struct stat *, stat_buf)
OVERRIDE_FUNCTION(3, 2, int, __lxstat, int, ver, const char *, path, struct stat *, stat_buf)
OVERRIDE_FUNCTION(3, 2, int, __xstat64, int, ver, const char *, path, struct stat64 *, stat_buf)
OVERRIDE_FUNCTION(3, 2, int, __lxstat64, int, ver, const char *, path, struct stat64 *, stat_buf)
#endif // DISABLE_XSTAT


#ifndef DISABLE_FSTATAT
OVERRIDE_FUNCTION(4, 2, int, fstatat, int, dirfd, const char *, pathname, struct stat *, statbuf, int, flags)
OVERRIDE_FUNCTION(4, 2, int, fstatat64, int, dirfd, const char *, pathname, struct stat64 *, statbuf, int, flags)
OVERRIDE_FUNCTION(5, 3, int, __fxstatat, int, ver, int, dirfd, const char *, pathname, struct stat *, statbuf, int, flags)
OVERRIDE_FUNCTION(5, 3, int, __fxstatat64, int, ver, int, dirfd, const char *, pathname, struct stat64 *, statbuf, int, flags)
#endif // DISABLE_FSTATAT


#ifndef DISABLE_STATFS
OVERRIDE_FUNCTION(2, 1, int, statfs, const char *, path, struct statfs *, buf)
OVERRIDE_FUNCTION(2, 1, int, statvfs, const char *, path, struct statvfs *, buf)
OVERRIDE_FUNCTION(2, 1, int, statfs64, const char *, path, struct statfs64 *, buf)
OVERRIDE_FUNCTION(2, 1, int, statvfs64, const char *, path, struct statvfs64 *, buf)
#endif // DISABLE_STATFS


#ifndef DISABLE_ACCESS
OVERRIDE_FUNCTION(2, 1, int, access, const char *, pathname, int, mode)
OVERRIDE_FUNCTION(4, 2, int, faccessat, int, dirfd, const char *, pathname, int, mode, int, flags)
#endif // DISABLE_ACCESS


#ifndef DISABLE_XATTR
OVERRIDE_FUNCTION(4, 1, ssize_t, getxattr, const char *, path, const char *, name, void *, value, size_t, size)
OVERRIDE_FUNCTION(4, 1, ssize_t, lgetxattr, const char *, path, const char *, name, void *, value, size_t, size)
#endif // DISABLE_XATTR


#ifndef DISABLE_OPENDIR
OVERRIDE_FUNCTION(1, 1, DIR *, opendir, const char *, name)
#endif // DISABLE_OPENDIR


#ifndef DISABLE_PATHCONF
OVERRIDE_FUNCTION(2, 1, long, pathconf, const char *, path, int, name)
#endif // DISABLE_PATHCONF


#ifndef DISABLE_REALPATH
OVERRIDE_FUNCTION(2, 1, char *, realpath, const char *, path, char *, resolved_path)
OVERRIDE_FUNCTION(1, 1, char *, canonicalize_file_name, const char *, path)
#endif // DISABLE_REALPATH


#ifndef DISABLE_READLINK
OVERRIDE_FUNCTION(3, 1, ssize_t, readlink, const char *, pathname, char *, buf, size_t, bufsiz)
OVERRIDE_FUNCTION(4, 2, ssize_t, readlinkat, int, dirfd, const char *, pathname, char *, buf, size_t, bufsiz)
#endif // DISABLE_READLINK


#ifndef DISABLE_SYMLINK
OVERRIDE_FUNCTION(2, 2, int, symlink, const char *, target, const char *, linkpath)
OVERRIDE_FUNCTION(3, 3, int, symlinkat, const char *, target, int, newdirfd, const char *, linkpath)
#endif // DISABLE_SYMLINK


#ifndef DISABLE_CHMOD
OVERRIDE_FUNCTION(2, 1, int, chmod, const char *, pathname, mode_t, mode)
OVERRIDE_FUNCTION(4, 2, int, fchmodat, int, dirfd, const char *, pathname, mode_t, mode, int, flags)
#endif // DISABLE_CHMOD


#ifndef DISABLE_CHOWN
OVERRIDE_FUNCTION(3, 1, int, chown, const char *, pathname, uid_t, owner, gid_t, group)
OVERRIDE_FUNCTION(3, 1, int, lchown, const char *, pathname, uid_t, owner, gid_t, group)
OVERRIDE_FUNCTION(5, 2, int, fchownat, int, dirfd, const char *, pathname, uid_t, owner, gid_t, group, int, flags)
#endif // DISABLE_CHOWN


#ifndef DISABLE_UNLINK
OVERRIDE_FUNCTION(1, 1, int, unlink, const char *, pathname)
OVERRIDE_FUNCTION(3, 2, int, unlinkat, int, dirfd, const char *, pathname, int, flags)
OVERRIDE_FUNCTION(1, 1, int, rmdir, const char *, pathname)
OVERRIDE_FUNCTION(1, 1, int, remove, const char *, pathname)
#endif // DISABLE_UNLINK


#ifndef DISABLE_EXEC
OVERRIDE_FUNCTION(2, 1, int, execv, const char *, filename, char * const*, argv)
OVERRIDE_FUNCTION(3, 1, int, execve, const char *, filename, char * const*, argv, char * const*, env)
OVERRIDE_FUNCTION(2, 1, int, execvp, const char *, filename, char * const*, argv)

int execl(const char *filename, const char *arg0, ...)
{
    debug_fprintf(stderr, "execl(%s) called\n", filename);

    char buffer[MAX_PATH];
    const char *new_path = fix_path(filename, buffer, sizeof buffer);

    // Note: call execv, not execl, because we can't call varargs functions with an unknown number of args
    static orig_execv_func_type execv_func = NULL;
    if (execv_func == NULL) {
        execv_func = (orig_execv_func_type)dlsym(RTLD_NEXT, "execv");
    }

    // count args
    int argc = 1;
    va_list args_list;
    va_start(args_list, arg0);
    while (va_arg(args_list, char *) != NULL) argc += 1;
    va_end(args_list);

    // extract args
    const char **argv_buffer = malloc(sizeof(char *) * (argc + 1));
    va_start(args_list, arg0);
    argv_buffer[0] = arg0;
    argc = 1;
    char *arg = NULL;
    while ((arg = va_arg(args_list, char *)) != NULL) {
        argv_buffer[argc++] = arg;
    }
    va_end(args_list);
    argv_buffer[argc] = NULL;

    return execv_func(new_path, (char * const*)argv_buffer);
}

int execlp(const char *filename, const char *arg0, ...)
{
    debug_fprintf(stderr, "execlp(%s) called\n", filename);

    char buffer[MAX_PATH];
    const char *new_path = fix_path(filename, buffer, sizeof buffer);

    // Note: call execvp, not execlp, because we can't call varargs functions with an unknown number of args
    static orig_execvp_func_type execvp_func = NULL;
    if (execvp_func == NULL) {
        execvp_func = (orig_execvp_func_type)dlsym(RTLD_NEXT, "execvp");
    }

    // count args
    int argc = 1;
    va_list args_list;
    va_start(args_list, arg0);
    while (va_arg(args_list, char *) != NULL) argc += 1;
    va_end(args_list);

    // extract args
    const char **argv_buffer = malloc(sizeof(char *) * (argc + 1));
    va_start(args_list, arg0);
    argv_buffer[0] = arg0;
    argc = 1;
    char *arg = NULL;
    while ((arg = va_arg(args_list, char *)) != NULL) {
        argv_buffer[argc++] = arg;
    }
    va_end(args_list);
    argv_buffer[argc] = NULL;

    return execvp_func(new_path, (char * const*)argv_buffer);
}

int execle(const char *filename, const char *arg0, ... /* , char *const env[] */)
{
    debug_fprintf(stderr, "execl(%s) called\n", filename);

    char buffer[MAX_PATH];
    const char *new_path = fix_path(filename, buffer, sizeof buffer);

    // Note: call execve, not execle, because we can't call varargs functions with an unknown number of args
    static orig_execve_func_type execve_func = NULL;
    if (execve_func == NULL) {
        execve_func = (orig_execve_func_type)dlsym(RTLD_NEXT, "execve");
    }

    // count args
    int argc = 1;
    va_list args_list;
    va_start(args_list, arg0);
    while (va_arg(args_list, char *) != NULL) argc += 1;
    va_end(args_list);

    // extract args
    const char **argv_buffer = malloc(sizeof(char *) * (argc + 1));
    va_start(args_list, arg0);
    argv_buffer[0] = arg0;
    argc = 1;
    char *arg = NULL;
    while ((arg = va_arg(args_list, char *)) != NULL) {
        argv_buffer[argc++] = arg;
    }
    char * const* env = va_arg(args_list, char * const*);
    va_end(args_list);
    argv_buffer[argc] = NULL;

    return execve_func(new_path, (char * const*)argv_buffer, env);
}
#endif // DISABLE_EXEC


#ifndef DISABLE_RENAME
typedef int (*orig_rename_func_type)(const char *oldpath, const char *newpath);
int rename(const char *oldpath, const char *newpath)
{
    debug_fprintf(stderr, "rename(%s) called\n", pathname);

    char buffer[MAX_PATH], buffer2[MAX_PATH];
    const char *new_oldpath = fix_path(oldpath, buffer, sizeof buffer);
    const char *new_newpath = fix_path(newpath, buffer2, sizeof buffer2);

    static orig_rename_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_rename_func_type)dlsym(RTLD_NEXT, "rename");
    }

    return orig_func(new_oldpath, new_newpath);
}

typedef int (*orig_renameat_func_type)(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath)
{
    debug_fprintf(stderr, "renameat(%s) called\n", pathname);

    char buffer[MAX_PATH], buffer2[MAX_PATH];
    const char *new_oldpath = fix_path(oldpath, buffer, sizeof buffer);
    const char *new_newpath = fix_path(newpath, buffer2, sizeof buffer2);

    static orig_renameat_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_renameat_func_type)dlsym(RTLD_NEXT, "renameat");
    }

    return orig_func(olddirfd, new_oldpath, newdirfd, new_newpath);
}

typedef int (*orig_renameat2_func_type)(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, unsigned int flags);
int renameat2(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, unsigned int flags)
{
    debug_fprintf(stderr, "renameat2(%s) called\n", pathname);

    char buffer[MAX_PATH], buffer2[MAX_PATH];
    const char *new_oldpath = fix_path(oldpath, buffer, sizeof buffer);
    const char *new_newpath = fix_path(newpath, buffer2, sizeof buffer2);

    static orig_renameat2_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_renameat2_func_type)dlsym(RTLD_NEXT, "renameat2");
    }

    return orig_func(olddirfd, new_oldpath, newdirfd, new_newpath, flags);
}
#endif // DISABLE_RENAME

