#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "ylog.h"
#include "ydb.h"

char *example_yaml =
    "ge%d:\n"
    " description: \n"
    " enabled: true\n"
    " ipv4:\n"
    "  address:\n"
    "   192.168.77.%d:\n"
    "    prefix-length: 16\n"
    "    secondary: false\n"
    " link-up-down-trap-enable: disabled\n"
    " name: ge%d\n"
    " type: mgmt\n";


ydb_res update_hook(ydb *datablock, char *path, FILE *fp)
{
    static int enabled;
    printf("HOOK %s path=%s\n", __func__, path);
    enabled = (enabled + 1) % 2;
    fprintf(fp,
            "ge1:\n"
            " enabled: %s\n",
            enabled ? "true" : "false");
    return YDB_OK;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast" // disable casting warning.
ydb_res update_hook1(ydb *datablock, char *path, FILE *fp, void *U1)
{
    static int enabled;
    int n = (int) U1;
    printf("HOOK %s path=%s\n", __func__, path);
    enabled = (enabled + 1) % 2;
    fprintf(fp,
            "ge%d:\n"
            " enabled: %s\n",
            n,
            enabled ? "true" : "false");
    return YDB_OK;
}
#pragma GCC diagnostic pop

void notify_hook(ydb *datablock, char op, ydb_iter *cur, ydb_iter *_new)
{
    printf("HOOK %s (%c) cur={%s:%s} new={%s:%s}\n", __func__, op,
           ydb_key(cur) ? ydb_key(cur) : "",
           ydb_value(cur) ? ydb_value(cur) : "",
           ydb_key(_new) ? ydb_key(_new) : "",
           ydb_value(_new) ? ydb_value(_new) : "");
}

int test_hook()
{
    ydb_res res = YDB_OK;
    printf("\n\n=== %s ===\n", __func__);
    ydb *datablock;
    datablock = ydb_open("top");
    if (!datablock)
    {
        fprintf(stderr, "ydb_open failed.\n");
        goto _done;
    }

    int n;
    for (n = 1; n <= 3; n++)
    {
        char buf[1024];
        sprintf(buf, example_yaml, n, n, n);
        res = ydb_parses(datablock, buf, strlen(buf));
        if (res)
            goto _done;
    }

    ydb_read_hook_add(datablock, "/ge1/enabled", (ydb_read_hook)update_hook, 0);
    ydb_write_hook_add(datablock, "/ge1", (ydb_write_hook)notify_hook, NULL, 0);

    char enabled[32] = {0};
    ydb_read(datablock, "ge1: {enabled: %s}\n", enabled);
    printf("/ge1/enabled=%s\n", enabled);

    ydb_fprintf(stdout, datablock,
                "ge1: {enabled}\n"
                "ge2: {enabled}\n"
                "ge3:\n");
    ydb_dump(datablock, stdout);
_done:
    if (res)
        fprintf(stderr, "%s failed. (%s)\n", __func__, ydb_res_str[res]);
    ydb_close(datablock);
    return res;
}

static int done;
void HANDLER_SIGINT(int param)
{
    done = 1;
}

int test_remote_hook(int n)
{
    ydb_res res = YDB_OK;
    printf("\n\n=== %s ===\n", __func__);
    ydb *datablock;

    // ylog_file_open("ydb-remote-hook-%d.log", n);
    datablock = ydb_open("top");
    if (!datablock)
    {
        fprintf(stderr, "ydb_open failed.\n");
        goto _done;
    }

    // char buf[1024];
    // sprintf(buf, example_yaml, n, n, n);
    // res = ydb_parses(datablock, buf, strlen(buf));
    // if (res)
    //     goto _done;
        
    res = ydb_connect(datablock, NULL, "pub:sync");
    if (res)
        goto _done;

    char path[64];
    sprintf(path, "/ge%d", n);
    ydb_read_hook_add(datablock, path, (ydb_read_hook)update_hook1, 1, n);

    // ignore SIGPIPE.
    signal(SIGPIPE, SIG_IGN);
    // add a signal handler to quit this program.
    signal(SIGINT, HANDLER_SIGINT);
    do
    {
        res = ydb_serve(datablock, 10000);
        if (res)
            goto _done;
    } while (!done);

_done:
    if (res)
        fprintf(stderr, "%s failed. (%s)\n", __func__, ydb_res_str[res]);
    ydb_close(datablock);
    // ylog_file_close();
    return res;
}

int main(int argc, char *argv[])
{
    if (argc >= 2)
    {
        ylog_severity = YLOG_INFO;
        test_remote_hook(atoi(argv[1]));
    }
    else
    {
        ylog_severity = YLOG_DEBUG;
        test_hook();
    }
    return 0;
}
