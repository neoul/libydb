// ydb-ipc-sub.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

#include "ylog.h"
#include "ydb.h"

static int done;
void HANDLER_SIGINT(int param)
{
    done = 1;
}

int main(int argc, char *argv[])
{
    ydb_res res = YDB_OK;
    ydb *datablock;

    datablock = ydb_open("subscriber");
    if (!datablock)
    {
        fprintf(stderr, "ydb_open failed.\n");
        goto _done;
    }
    res = ydb_connect(datablock, "us:///tmp/test", "sub");
    if (res)
        goto _done;
    
    // ignore SIGPIPE.
    signal(SIGPIPE, SIG_IGN);
    // add a signal handler to quit this program.
    signal(SIGINT, HANDLER_SIGINT);
    do
    {
        res = ydb_serve(datablock, 2000);
        if (YDB_FAILED(res))
        {
            fprintf(stderr, "error: %s\n", ydb_res_str(res));
            goto _done;
        }
        else if (YDB_WARNING(res))
            fprintf(stdout, "warning: %s\n", ydb_res_str(res));
        break;
    } while (!done);

_done:
    if (res)
        fprintf(stderr, "%s failed. (%s)\n", __func__, ydb_res_str(res));
    ydb_dump(datablock, stdout);
    ydb_close(datablock);
    return res;
}
