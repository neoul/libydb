#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "ymldb.h"

char *example = \
    "books:\n" \
    " python: %d\n" \
    " java: %d\n" \
    " c++: %d\n" \
    " c: %d\n" \
    " java-script: %d\n";

int push_and_pull()
{
    ymldb_log_set(YMLDB_LOG_LOG, NULL);
    
    // create ymldb for interface.
    int res = ymldb_create("example", YMLDB_FLAG_NONE);
    if (res < 0)
    {
        fprintf(stderr, "ymldb create failed.\n");
        return -1;
    }

    ymldb_push("example", example, 10, 20, 30, 40, 50);
    // ymldb_dump(stdout, "example");
    
    int python = 0;
    int java = 0;
    int cpp = 0;
    int c = 0;
    int javascript = 0;
    ymldb_pull("example", example, &python, &java, &cpp, &c, &javascript);

    printf("p=%d, j=%d, cpp=%d, c=%d, js=%d\n", python, java, cpp, c, javascript);
    ymldb_destroy_all();
    return 0;
}

int main(int argc, char *argv[])
{

    return push_and_pull();
}
