// ydb-ipc-read-hook.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "ylog.h"
#include "ydb.h"

const char *example_yaml =
    "# YAML data with operation (merge/ delete)\n"
    "system:\n"
    "  interface:\n"
    "   eth0:\n"
    "    enable: %s\n"
    "    ip: 10.10.1.1\n";

static int done;
void HANDLER_SIGINT(int param)
{
    done = 1;
}

ydb_res read_hook(ydb *datablock, char *path, FILE *fp)
{
    static int enabled;
    // printf("read_hook path=%s\n", path);
    enabled = (enabled + 1) % 2;
    fprintf(fp, example_yaml, enabled ? "true" : "false");
    return YDB_OK;
}

int main(int argc, char *argv[])
{
    ydb_res res = YDB_OK;
    ydb *datablock;

    if (argc > 1 && strcmp(argv[1], "subscriber") == 0)
    {
        datablock = ydb_open("subscriber");
        if (!datablock)
        {
            fprintf(stderr, "ydb_open failed.\n");
            goto _done;
        }
        res = ydb_connect(datablock, "us:///tmp/test", "sub");
        if (res)
            goto _done;
    }
    else
    {
        datablock = ydb_open("publisher");
        if (!datablock)
        {
            fprintf(stderr, "ydb_open failed.\n");
            goto _done;
        }
        ydb_read_hook_add(datablock, "/", (ydb_read_hook)read_hook, 0);
        res = ydb_connect(datablock, "us:///tmp/test", "pub");
        if (res)
            goto _done;
    }
    

    // ignore SIGPIPE.
    signal(SIGPIPE, SIG_IGN);
    // add a signal handler to quit this program.
    signal(SIGINT, HANDLER_SIGINT);
    do
    {
        if (argc > 1 && strcmp(argv[1], "subscriber") == 0)
        {
            ydb_sync(datablock, "/system");
            ydb_serve(datablock, 1000);
            break;
        }
        else
        {
            res = ydb_serve(datablock, 0);
            if (YDB_FAILED(res))
            {
                fprintf(stderr, "error: %s\n", ydb_res_str(res));
                goto _done;
            }
            else if (YDB_WARNING(res))
                fprintf(stdout, "warning: %s\n", ydb_res_str(res));
        }
        
    } while (!done);

_done:
    ydb_dump(datablock, stdout);
    if (res)
        fprintf(stderr, "%s failed. (%s)\n", __func__, ydb_res_str(res));
    ydb_close(datablock);
    return res;
}
