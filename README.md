# ld-preload-open / path-mapping.so
This library can trick a process into opening absolute paths from a different location, similar to bind mounts, but without root access.

> **WARNING:** This is kind of a hack.
> It works quite well, mostly, but unforseen side effects or crashes can not be completely prevented.
> ***DO NOT USE for mission critical software!***

## Example 1

One example are ["Environment Modules"](https://modules.readthedocs.io/en/latest/) which can be loaded with the command `module load` to activate some software in the current shell. If you have multiple versions of a software, which should be available side by side, you can use this library to trick them into loading version specific assets from a common absolute path.

Imagine you are an admin and would like to provide both version 2019 and version 2021 of `someprogram` to your users.
However, `someprogram` loads a file from a hard-coded path `/usr/share/someprogram/assets`, but different versions of the program ship with different versions of the file `/usr/share/someprogram/assets`.

By creating a wrapper script which runs the original program with `path-mapping.so`, you can force each version to load its own version, without altering the executable binary:

```bash
#!/bin/bash
# in file /modules/someprogram/v2019/wrapper/someprogram
PATH_MAPPING="/usr/share/someprogram:/modules/someprogram/v2019/share/someprogram" \
  LD_PRELOAD=/path/to/path-mapping.so \
  /modules/someprogram/v2019/bin/someprogram
```

Then just add `/modules/someprogram/v2019/wrapper` to `PATH` in your module definition file for version 2019 of `someprogram` and your users will be able to load it with `module load someprogram/2019` and run the wrapper script as `someprogram`.

## Example 2

Another example use-case might be to run a program with hard-coded paths from your home folder.
Imagine, `someprogram` tries to load files from `/usr/share/someprogram`, with no way to configure that path (apart from re-compiling, *if* you have the source code at all).
If you can't put the files there (for whatever reason), you could place them in `$HOME/.local/share/someprogram` instead.

With the following command, when the program tries to open e.g. `/usr/share/someprogram/icons.png` (which does not exist),
`path-mapping.so` would intercept that file access and rewrite the path to `$HOME/.local/share/someprogram/icons.png`, which does exist:
```bash
PATH_MAPPING="/usr/share/someprogram:$HOME/.local/share/someprogram" \
  LD_PRELOAD=/path/to/path-mapping.so \
  someprogram
```

## How it works

The path mapping works by intercepting standard library functions which have a path argument, like `open` or `chmod`.
If this argument matches the given prefix (the first part) of the mapping, the prefix is replaced by the destination (the second part) of the mapping.
Then the original standard library function is called with this possibly modified path.

Most Linux `libc` functions that do something with files are supported (except those that I forgot).
The actual list of functions is quite long and can be looked up in the code.
However, even if all functions with a `path` argument are overloaded, there are some pitfalls.
See below under **Potential problems** for more information.

## Path mapping configuration

There are two ways to specify the path mappings. An arbitrary number of mappings can be used at once.

1. If the environment variable `PATH_MAPPING` exists, path-mapping.so will try to initialize the mappins from there.
   The first part of each pair is the prefix, and the second part is the destination, so the number of given paths must be even.
   All parts are separated by colons:
   ```bash
   export PATH_MAPPING="/usr/virtual1:/map/dest1:/usr/virtual2:/map/dest2"
   ```

2. If `PATH_MAPPING` is unset or empty, the mapping specified in the variable `default_path_map` will be used instead.
   You can modify it if you don't want to set `PATH_MAPPING`, for example like this:
   ```C
   static const char *default_path_map[][2] = {
       { "/usr/virtual1", "/map/dest1" },
       { "/usr/virtual1", "/map/dest2" },
   };
   ```

## Compiling and installation

Just run `make` to compile `path-mapping.so`.
Place this file anywhere you like and provide its absolute path to `LD_PRELOAD=/absolute/path/to/path-mapping.so` to use it.

## Compile time options

There some options which can be set during compile time by defining some preprocessor macros.
For example, adding `#define QUIET` or compiling with `-DQUIET` will remove all informational printf commands.

Normally, the loaded path mapping will be printed to `stderr` at startup, and an info message will be printed to `stderr` each time a path mapping is applied.

* `QUIET`: Removes all `info_fprintf` commands.
  The resulting `path-mapping.so` will not print anything, except for initialization errors which will `exit()` the program immediately.
* `DEBUG`: Enables all `debug_fprintf` calls.
  This will print additional output to `stderr` each time an overloaded functions called, including the path argument(s).
* `DISABLE_*`: These options allow you to disable the overloading of some specific functions if you desire.
  See the code in `path-mapping.c` for a complete list.

## Tests

Run `make test` to execute the included test suite.
Most things should be tested, but multiple variants of the same function are usually not tested separately.

## Potential problems

On first glance, this library might look like it can be used as a replacement for `mount --bind`.
However, since this is quite a hacky solution that runs only in user space, there are some issues where things do not work quite as one would expect.
Some of these could be fixed or worked around, but in some cases that would require significantly more work than just overloading a few functions.

1. Only absolute paths are currently mapped, relative paths are not.
   If a program does `open("/usr/virtual1/file")`, it will be mapped to `/map/dest1/file`, but `chdir("/usr")` followed by `open("virtual1/file")` will fail with `ENOENT`.

   The same problem applies to functions ending in `at`, like `openat`.
   These functions have a parameter `int dirfd`, relative to which the `path` argument is searched (if it is not an absolute path).
   This library makes no attempt to find out the path of the directory which `dirfd` represents.
2. Return values from standard library functions are not mapped.
   For example, `getcwd()` will return `/map/dest1` after a calling `chdir("/usr/virtual1")` (from the example above).

   However, this is usually not be a problem, because the program can then internally use that existing path for all future accesses, which will succeed as expected.
   Even an interactive `bash` session can work (to a certain extent) inside virtual mapped directories.
3. Virtual mapped entries do not appear in directory listings.
   The example mapping for `/usr/virtual1` will not show up in `ls /usr` or `find /usr`.
4. Symlinks that point into virtual directories will not work, because symlinks are evaluated by the kernel, not in user space.
   For example, the following will fail:
   ```bash
   export PATH_MAPPING=/tmp/virtual:/tmp/real
   export LD_PRELOAD=/path/to/path-mapping.so
   mkdir /tmp/real
   touch /tmp/real/file
   ln -s /tmp/virtual /tmp/link
   ls -l /tmp/virtual/file # works
   ls -l /tmp/link/file # fails because kernel can not see `/tmp/virtual`
   ```
5. Creating relative symlinks that cross a mapping boundary will not work as expected:
   ```bash
   export PATH_MAPPING=/tmp/1/virtual:/tmp/real
   export LD_PRELOAD=/path/to/path-mapping.so
   mkdir /tmp/real
   echo content >/tmp/realfile
   ln -s ../../realfile /tmp/1/virtual/virtuallink
   cat /tmp/1/virtual/virtuallink # fails because /realfile does not exist
   ```
   The created link *would* point to `/tmp/realfile`, if `/tmp/1/virtual/` was a real directory.
   But since the symlink is evaluated relative to `/tmp/real`, it will actually point to `/realfile`, which does not exist.
6. If a programs manually loads a function like `fopen` from `libc.so` using `ldopen` and `dlsym`, then `LD_PRELOAD` can not intercept that.
   In this case, the path mapping will not work.
7. If a standard library function internally calls an overloaded function like `stat` or `open`, then `LD_PRELOAD` can not intercept that.
8. If internal workings of the libc change in the future, a program might just stop working.
9. Path mapping does not work if a program talks to the kernel directly using syscalls (which would be *very* bad practice) instead of using the `libc` functions to make the syscalls for it.
   Or if a program uses a different standard library, which does syscalls directly instead of falling back to the standard `libc` functions (not sure if something like that exists in practice).

## License

This repository is released unter the MIT license, see the file LICENSE for details.