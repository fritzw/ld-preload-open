/*
MIT License

Copyright (c) 2022 Fritz Webering

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h> // exit
#include <dlfcn.h> // dlsym
#include <fcntl.h> // stat
#include <dirent.h> // DIR*
#include <stdarg.h> // va_start, va_arg
#include <sys/vfs.h> // statfs
#include <sys/statvfs.h> // statvfs
#include <unistd.h> // uid_t, gid_t
#include <malloc.h> // for execl
#include <utime.h> // utimebuf
#include <sys/time.h> // struct timeval
#include <sys/types.h> // dev_t
#include <ftw.h> // ftw
#include <fts.h> // fts
#include <assert.h>

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

// Enable or disable specific overrides (always includes different variants and the 64 version if applicable)
// #define DISABLE_OPEN
// #define DISABLE_OPENAT
// #define DISABLE_FOPEN
// #define DISABLE_CHDIR
// #define DISABLE_STAT
// #define DISABLE_FSTATAT
// #define DISABLE_STATFS
// #define DISABLE_XSTAT
// #define DISABLE_ACCESS
// #define DISABLE_XATTR
// #define DISABLE_OPENDIR
// #define DISABLE_MKDIR
// #define DISABLE_FTW
// #define DISABLE_FTS
// #define DISABLE_PATHCONF
// #define DISABLE_REALPATH
// #define DISABLE_READLINK
// #define DISABLE_SYMLINK
// #define DISABLE_MKFIFO
// #define DISABLE_MKNOD
// #define DISABLE_UTIME
// #define DISABLE_CHMOD
// #define DISABLE_CHOWN
// #define DISABLE_UNLINK
// #define DISABLE_EXEC
// #define DISABLE_RENAME
// #define DISABLE_LINK

// List of path pairs. Paths beginning with the first item will be
// translated by replacing the matching part with the second item.
static const char *default_path_map[][2] = {
    { "/tmp/path-mapping/tests/virtual", "/tmp/path-mapping/tests/real" },
};

static const char *(*path_map)[2] = default_path_map;
static int path_map_length = (sizeof default_path_map) / (sizeof default_path_map[0]);
static char *path_map_buffer = NULL;


//////////////////////////////////////////////////////////
// Constructor to inspect the PATH_MAPPING env variable //
//////////////////////////////////////////////////////////


__attribute__((constructor))
static void path_mapping_init()
{
    if (path_map != default_path_map) return;

    // If environment variable is set and non-empty, override the default
    const char *env_string = getenv("PATH_MAPPING");
    if (env_string != NULL && strlen(env_string) > 0) {

        // Allocate a buffer to store the entries of the map in one big block, separated by null bytes
        size_t buffersize = strlen(env_string) + 1;
        path_map_buffer = malloc(buffersize);
        if (path_map_buffer == NULL) {
            error_fprintf(stderr, "PATH_MAPPING out of memory\n");
            exit(255);
        }
        strncpy(path_map_buffer, env_string, buffersize);
        path_map_buffer[buffersize] = '\0';

        // Count the number of separators ':' to determine the size of the array of pointers
        int n_segments = 1;
        for (int i = 0; env_string[i]; i++) {
            if (env_string[i] == ':') n_segments++;
        }
        if (n_segments % 2 != 0) {
            error_fprintf(stderr, "PATH_MAPPING must have an even number of parts, not %d\n", n_segments);
            exit(255);
        }

        // Allocate memory for the actual array of pointers to the map entries
        path_map = malloc(n_segments * 2 * sizeof(char*));
        if (path_map == NULL) {
            error_fprintf(stderr, "PATH_MAPPING out of memory\n");
            exit(255);
        }

        // Split the large string buffer into smaller strings by replacing ':' with null bytes
        char **linear_path_map = (char **)path_map;
        int linear_index = 0;
        linear_path_map[linear_index++] = path_map_buffer;
        for (int i = 0; path_map_buffer[i]; i++) {
            if (path_map_buffer[i] == ':') {
                path_map_buffer[i] = '\0';
                linear_path_map[linear_index++] = &(path_map_buffer[i+1]);
            }
        }
        path_map_length = linear_index / 2;
        assert(linear_index == n_segments);
    }

    for (int i = 0; i < path_map_length; i++) {
        info_fprintf(stderr, "PATH_MAPPING: %s => %s\n", path_map[i][0], path_map[i][1]);
    }
}

__attribute__((destructor))
static void path_mapping_deinit()
{
    if (path_map != default_path_map) {
        free(path_map);
    }
    free(path_map_buffer);
}


/////////////////////////////////////////////////////////
//   Helper functions to do the actual path mapping    //
/////////////////////////////////////////////////////////


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
    if (path == NULL) return path;

    for (int i = 0; i < path_map_length; i++) {
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


/////////////////////////////////////////////////////////
// Macro definitions for generating function overrides //
/////////////////////////////////////////////////////////


// Hint for debugging these macros:
// Remove the #define __NL__, then compile with gcc -save-temps.
// Then open path-mapping.i with a text editor and replace __NL__ with newlines.
#define __NL__

// Select argument name i from the variable argument list (ignoring types)
#define OVERRIDE_ARG(i, ...)  OVERRIDE_ARG_##i(__VA_ARGS__)
#define OVERRIDE_ARG_1(type1, arg1, ...)  arg1
#define OVERRIDE_ARG_2(type1, arg1, type2, arg2, ...)  arg2
#define OVERRIDE_ARG_3(type1, arg1, type2, arg2, type3, arg3, ...)  arg3
#define OVERRIDE_ARG_4(type1, arg1, type2, arg2, type3, arg3, type4, arg4, ...)  arg4
#define OVERRIDE_ARG_5(type1, arg1, type2, arg2, type3, arg3, type4, arg4, arg5, ...)  arg5

// Create the function pointer typedef for the function
#define OVERRIDE_TYPEDEF_NAME(funcname) orig_##funcname##_func_type
#define OVERRIDE_TYPEDEF(has_varargs, nargs, returntype, funcname, ...)  typedef returntype (*OVERRIDE_TYPEDEF_NAME(funcname))(OVERRIDE_ARGS(has_varargs, nargs, __VA_ARGS__));

// Create a valid C argument list including types
#define OVERRIDE_ARGS(has_varargs, nargs, ...)  OVERRIDE_ARGS_##nargs(has_varargs, __VA_ARGS__)
#define OVERRIDE_ARGS_1(has_varargs, type1, arg1)  type1 arg1 OVERRIDE_VARARGS(has_varargs)
#define OVERRIDE_ARGS_2(has_varargs, type1, arg1, type2, arg2)  type1 arg1, type2 arg2 OVERRIDE_VARARGS(has_varargs)
#define OVERRIDE_ARGS_3(has_varargs, type1, arg1, type2, arg2, type3, arg3)  type1 arg1, type2 arg2, type3 arg3 OVERRIDE_VARARGS(has_varargs)
#define OVERRIDE_ARGS_4(has_varargs, type1, arg1, type2, arg2, type3, arg3, type4, arg4)  type1 arg1, type2 arg2, type3 arg3, type4 arg4 OVERRIDE_VARARGS(has_varargs)
#define OVERRIDE_ARGS_5(has_varargs, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5)  type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5 OVERRIDE_VARARGS(has_varargs)
// Print ", ..." in the argument list if has_varargs is 1
#define OVERRIDE_VARARGS(has_varargs) OVERRIDE_VARARGS_##has_varargs
#define OVERRIDE_VARARGS_0
#define OVERRIDE_VARARGS_1 , ...

// Create an argument list without types where one argument is replaced with new_path
#define OVERRIDE_RETURN_ARGS(nargs, path_arg_pos, ...)  OVERRIDE_RETURN_ARGS_##nargs##_##path_arg_pos(__VA_ARGS__)
#define OVERRIDE_RETURN_ARGS_1_1(type1, arg1)  new_path
#define OVERRIDE_RETURN_ARGS_2_1(type1, arg1, type2, arg2)  new_path, arg2
#define OVERRIDE_RETURN_ARGS_2_2(type1, arg1, type2, arg2)  arg1, new_path
#define OVERRIDE_RETURN_ARGS_3_1(type1, arg1, type2, arg2, type3, arg3)  new_path, arg2, arg3
#define OVERRIDE_RETURN_ARGS_3_2(type1, arg1, type2, arg2, type3, arg3)  arg1, new_path, arg3
#define OVERRIDE_RETURN_ARGS_3_3(type1, arg1, type2, arg2, type3, arg3)  arg1, arg2, new_path
#define OVERRIDE_RETURN_ARGS_4_1(type1, arg1, type2, arg2, type3, arg3, type4, arg4)  new_path, arg2, arg3, arg4
#define OVERRIDE_RETURN_ARGS_4_2(type1, arg1, type2, arg2, type3, arg3, type4, arg4)  arg1, new_path, arg3, arg4
#define OVERRIDE_RETURN_ARGS_4_3(type1, arg1, type2, arg2, type3, arg3, type4, arg4)  arg1, arg2, new_path, arg4
#define OVERRIDE_RETURN_ARGS_4_4(type1, arg1, type2, arg2, type3, arg3, type4, arg4)  arg1, arg2, arg3, new_path
#define OVERRIDE_RETURN_ARGS_5_1(type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5)  new_path, arg2, arg3, arg4, arg5
#define OVERRIDE_RETURN_ARGS_5_2(type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5)  arg1, new_path, arg3, arg4, arg5
#define OVERRIDE_RETURN_ARGS_5_3(type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5)  arg1, arg2, new_path, arg4, arg5
#define OVERRIDE_RETURN_ARGS_5_4(type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5)  arg1, arg2, arg3, new_path, arg5
#define OVERRIDE_RETURN_ARGS_5_5(type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5)  arg1, arg2, arg3, arg4, new_path

// Use this to override a function without varargs
#define OVERRIDE_FUNCTION(nargs, path_arg_pos, returntype, funcname, ...) \
    OVERRIDE_FUNCTION_MODE_GENERIC(0, nargs, path_arg_pos, returntype, funcname, __VA_ARGS__)

// Use this to override a function with a vararg mode that works like open() or openat()
#define OVERRIDE_FUNCTION_VARARGS(nargs, path_arg_pos, returntype, funcname, ...) \
    OVERRIDE_FUNCTION_MODE_GENERIC(1, nargs, path_arg_pos, returntype, funcname, __VA_ARGS__)

#define OVERRIDE_FUNCTION_MODE_GENERIC(has_varargs, nargs, path_arg_pos, returntype, funcname, ...) \
OVERRIDE_TYPEDEF(has_varargs, nargs, returntype, funcname, __VA_ARGS__) \
__NL__ returntype funcname (OVERRIDE_ARGS(has_varargs, nargs, __VA_ARGS__))\
__NL__{\
__NL__    debug_fprintf(stderr, #funcname "(%s) called\n", OVERRIDE_ARG(path_arg_pos, __VA_ARGS__));\
__NL__    char buffer[MAX_PATH];\
__NL__    const char *new_path = fix_path(OVERRIDE_ARG(path_arg_pos, __VA_ARGS__), buffer, sizeof buffer);\
__NL__ \
__NL__    static OVERRIDE_TYPEDEF_NAME(funcname) orig_func = NULL;\
__NL__    if (orig_func == NULL) {\
__NL__        orig_func = (OVERRIDE_TYPEDEF_NAME(funcname))dlsym(RTLD_NEXT, #funcname);\
__NL__    }\
__NL__    OVERRIDE_DO_MODE_VARARG(has_varargs, nargs, path_arg_pos, __VA_ARGS__) \
__NL__    return orig_func(OVERRIDE_RETURN_ARGS(nargs, path_arg_pos, __VA_ARGS__));\
__NL__}

// Conditionally expands to the code used to handle the mode argument of open() and openat()
#define OVERRIDE_DO_MODE_VARARG(has_mode_vararg, nargs, path_arg_pos, ...) \
    OVERRIDE_DO_MODE_VARARG_##has_mode_vararg(nargs, path_arg_pos, __VA_ARGS__)
#define OVERRIDE_DO_MODE_VARARG_0(nargs, path_arg_pos, ...) // Do nothing
#define OVERRIDE_DO_MODE_VARARG_1(nargs, path_arg_pos, ...) \
__NL__    if (__OPEN_NEEDS_MODE(flags)) {\
__NL__        va_list args;\
__NL__        va_start(args, flags);\
__NL__        int mode = va_arg(args, int);\
__NL__        va_end(args);\
__NL__        return orig_func(OVERRIDE_RETURN_ARGS(nargs, path_arg_pos, __VA_ARGS__), mode);\
__NL__    }


/////////////////////////////////////////////////////////
//     Definition of all function overrides below      //
/////////////////////////////////////////////////////////


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


#ifndef DISABLE_CHDIR
OVERRIDE_FUNCTION(1, 1, int, chdir, const char *, path)
#endif // DISABLE_CHDIR


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


#ifndef DISABLE_MKDIR
OVERRIDE_FUNCTION(2, 1, int, mkdir, const char *, pathname, mode_t, mode)
#endif // DISABLE_MKDIR


#ifndef DISABLE_FTW
OVERRIDE_FUNCTION(3, 1, int, ftw, const char *, filename, __ftw_func_t, func, int, descriptors)
OVERRIDE_FUNCTION(4, 1, int, nftw, const char *, filename, __nftw_func_t, func, int, descriptors, int, flags)
OVERRIDE_FUNCTION(3, 1, int, ftw64, const char *, filename, __ftw64_func_t, func, int, descriptors)
OVERRIDE_FUNCTION(4, 1, int, nftw64, const char *, filename, __nftw64_func_t, func, int, descriptors, int, flags)
#endif // DISABLE_FTW


#ifndef DISABLE_FTS
typedef int (*fts_compare_func_t)(const FTSENT **, const FTSENT **);
typedef FTS* (*orig_fts_open_func_type)(char * const *path_argv, int options, fts_compare_func_t compare);
FTS *fts_open(char * const *path_argv, int options, fts_compare_func_t compare)
{
    if (path_argv[0] == NULL) return NULL;
    debug_fprintf(stderr, "fts_open(%s) called\n", path_argv[0]);

    FTS *result = NULL;
    int argc = 0;
    const char **new_paths;
    char **buffers;

    for (argc = 0; path_argv[argc] != NULL; argc++) {} // count number of paths in argument array

    buffers = malloc((argc + 1) * sizeof(char *));
    if (buffers == NULL) {
        goto _fts_open_return;
    }
    for (int i = 0; i < argc; i++) { buffers[i] = NULL; } // Initialize for free() in case of failure

    new_paths = malloc((argc + 1) * sizeof(char *));
    if (new_paths == NULL) {
        goto _fts_open_cleanup_buffers;
    }
    for (int i = 0; i < argc; i++) {
        buffers[i] = malloc(MAX_PATH + 1);
        if (buffers[i] == NULL) {
            goto _fts_open_cleanup;
        }
        new_paths[i] = fix_path(path_argv[i], buffers[i], MAX_PATH);
    }
    new_paths[argc] = NULL; // terminating null pointer

    static orig_fts_open_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_fts_open_func_type)dlsym(RTLD_NEXT, "fts_open");
    }

    result = orig_func((char * const *)new_paths, options, compare);

_fts_open_cleanup:
    for (int i = 0; i < argc; i++) {
        free(buffers[i]);
    }
    free(new_paths);
_fts_open_cleanup_buffers:
    free(buffers);
_fts_open_return:
    return result;
}
#endif // DISABLE_FTS


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


#ifndef DISABLE_MKFIFO
OVERRIDE_FUNCTION(2, 1, int, mkfifo, const char *, filename, mode_t, mode)
#endif // DISABLE_MKFIFO


#ifndef DISABLE_MKNOD
OVERRIDE_FUNCTION(3, 1, int, mknod, const char *, filename, mode_t, mode, dev_t, dev)
#endif // DISABLE_MKNOD


#ifndef DISABLE_UTIME
OVERRIDE_FUNCTION(2, 1, int, utime, const char *, filename, const struct utimbuf *, times)
OVERRIDE_FUNCTION(2, 1, int, utimes, const char *, filename, const struct timeval *, tvp)
OVERRIDE_FUNCTION(2, 1, int, lutime, const char *, filename, const struct utimbuf *, tvp)
OVERRIDE_FUNCTION(4, 2, int, utimensat, int, dirfd, const char *, pathname, const struct timespec *, times, int, flags)
OVERRIDE_FUNCTION(3, 2, int, futimesat, int, dirfd, const char *, pathname, const struct timeval *, times)
#endif // DISABLE_UTIME


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

    int result = execv_func(new_path, (char * const*)argv_buffer);
    free(argv_buffer); // We ONLY reach this if exec fails, so we need to clean up
    return result;
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

    int result = execvp_func(new_path, (char * const*)argv_buffer);
    free(argv_buffer); // We ONLY reach this if exec fails, so we need to clean up
    return result;
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

    int result = execve_func(new_path, (char * const*)argv_buffer, env);
    free(argv_buffer); // We ONLY reach this if exec fails, so we need to clean up
    return result;
}
#endif // DISABLE_EXEC


#ifndef DISABLE_RENAME
typedef int (*orig_rename_func_type)(const char *oldpath, const char *newpath);
int rename(const char *oldpath, const char *newpath)
{
    debug_fprintf(stderr, "rename(%s, %s) called\n", oldpath, newpath);

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
    debug_fprintf(stderr, "renameat(%s, %s) called\n", oldpath, newpath);

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
    debug_fprintf(stderr, "renameat2(%s, %s) called\n", oldpath, newpath);

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


#ifndef DISABLE_LINK
typedef int (*orig_link_func_type)(const char *oldpath, const char *newpath);
int link(const char *oldpath, const char *newpath)
{
    debug_fprintf(stderr, "link(%s, %s) called\n", oldpath, newpath);

    char buffer[MAX_PATH], buffer2[MAX_PATH];
    const char *new_oldpath = fix_path(oldpath, buffer, sizeof buffer);
    const char *new_newpath = fix_path(newpath, buffer2, sizeof buffer2);

    static orig_link_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_link_func_type)dlsym(RTLD_NEXT, "link");
    }

    return orig_func(new_oldpath, new_newpath);
}

typedef int (*orig_linkat_func_type)(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);
int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags)
{
    debug_fprintf(stderr, "linkat(%s, %s) called\n", oldpath, newpath);

    char buffer[MAX_PATH], buffer2[MAX_PATH];
    const char *new_oldpath = fix_path(oldpath, buffer, sizeof buffer);
    const char *new_newpath = fix_path(newpath, buffer2, sizeof buffer2);

    static orig_linkat_func_type orig_func = NULL;
    if (orig_func == NULL) {
        orig_func = (orig_linkat_func_type)dlsym(RTLD_NEXT, "linkat");
    }

    return orig_func(olddirfd, new_oldpath, newdirfd, new_newpath, flags);
}

#endif // DISABLE_LINK
