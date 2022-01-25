#define _POSIX_SOURCE
#include <utime.h>

int main(int argc, const char **argv)
{
    if (argc < 2) {
        return 1;
    }
    const char *filename = argv[1];
    struct utimbuf ubuf;

    ubuf.modtime = 100000000;
    ubuf.actime = 200000000;
    return utime(filename, &ubuf);
}
