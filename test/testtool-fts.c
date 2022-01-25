#include <fts.h>
#include <stdio.h>
#include <string.h>

int compare(const FTSENT** left, const FTSENT** right)
{
    return strcmp((*left)->fts_name, (*right)->fts_name);
}

int main(int argc, char* const argv[])
{
    if (argc < 2) return 1;

    FTS* ftsp = NULL;
    FTSENT *node = NULL;

    ftsp = fts_open(argv + 1, 0, &compare);

    if (ftsp != NULL) {
        while ( (node = fts_read(ftsp)) != NULL ) {
            printf("%s\n", node->fts_name);
        }
        fts_close(ftsp);
    }
    return 0;
}
