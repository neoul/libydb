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

// print changed data.
void write_hook(ydb *datablock, char op, ynode *base, ynode *cur, ynode *_new)
{
    fprintf(stdout, "write_hook op=%c cur={%s:%s} new={%s:%s}\n",
            op,
            ydb_key(cur) ? ydb_key(cur) : "",
            ydb_value(cur) ? ydb_value(cur) : "",
            ydb_key(_new) ? ydb_key(_new) : "",
            ydb_value(_new) ? ydb_value(_new) : "");
}

void write_supressed_hook(ydb *datablock, char op, ynode *base)
{
    fprintf(stdout, "write_supressed_hook op=%c\n", op);
    fprintf(stdout, "===========================\n");
    ydb_path_fprintf(stdout, datablock, "/");
    fprintf(stdout, "\n");
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
    // register YDB write hook to top node.
    if (argc > 1 && strcmp(argv[1], "supressed") == 0)
        ydb_write_hook_add(datablock, "/", 1, (ydb_write_hook) write_supressed_hook, 0);
    else
        ydb_write_hook_add(datablock, "/", 0, (ydb_write_hook) write_hook, 0);

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
    ydb_close(datablock);
    return res;
}
