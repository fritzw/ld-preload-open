#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <ftw.h>

static int print_filename(const char* filename, const struct stat *stats, int flags, struct FTW *ftwbuf)
{
	printf("%s\n", filename);
	return 0;
}

int main(int argc, const char **argv)
{
	if (argc != 2) return 1;
    return nftw(argv[1], print_filename, 20, 0);
}