#include <stdio.h>
#include <ftw.h>

static int print_filename(const char* filename, const struct stat *stats, int flags)
{
	printf("%s\n", filename);
	return 0;
}

int main(int argc, const char **argv)
{
	if (argc != 2) return 1;
    return ftw(argv[1], print_filename, 20);
}