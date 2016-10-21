# ld-preload-open
Library to map files or directories to another location, for use with LD_PRELOAD.
For example if an application tries to open files in `/etc/ownCloud/`, but you want it
to open the same file from `/home/user1/.etc/ownCloud/` instead.

Simply adjust the paths you want to map in the `.c` file and compile with `make`.
Then start your program with the following command:

    LD_PRELOAD=/path/to/my/path-mapping.so someprogram
