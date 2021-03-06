#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "ylog.h"
#include "ydb.h"

char *example_yaml =
    "interface[name=1/%d]:\n"
    " description: \n"
    " enabled: true\n"
    " ipv4:\n"
    "  address:\n"
    "   192.168.77.%d:\n"
    "    prefix-length: 16\n"
    "    secondary: false\n"
    " link-up-down-trap-enable: disabled\n"
    " name: 1/%d\n"
    " type: mgmt\n";


ydb_res update_hook(ydb *datablock, char *path, FILE *fp)
{
    static int enabled;
    printf("HOOK %s path=%s\n", __func__, path);
    enabled = (enabled + 1) % 2;
    fprintf(fp,
            "interface[name=1/1]:\n"
            " enabled: %s\n",
            enabled ? "true" : "false");
    return YDB_OK;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast" // disable casting warning.
ydb_res update_hook1(ydb *datablock, char *path, FILE *fp, void *U1)
{
    static int mtu = 1500;
    static int enabled;
    int n = (int) U1;
    printf("HOOK %s path=%s\n", __func__, path);
    enabled = (enabled + 1) % 2;
    fprintf(fp,
            "interfaces:\n"
            " interface[name=ge%d]:\n"
            "  enabled: %s\n"
            "  mtu: %d\n",
            n,
            enabled ? "true" : "false",
            (mtu++));
    return YDB_OK;
}
#pragma GCC diagnostic pop

void notify_hook(ydb *datablock, char op, ynode *base, ynode *cur, ynode *_new)
{
    char *path = ydb_path(datablock, base, NULL);
    fprintf(stdout, "HOOK %s (%c) cur={%s:%s} new={%s:%s}\n",
            path ? path: "unknown", op,
           ydb_key(cur) ? ydb_key(cur) : "",
           ydb_value(cur) ? ydb_value(cur) : "",
           ydb_key(_new) ? ydb_key(_new) : "",
           ydb_value(_new) ? ydb_value(_new) : "");
    if (path)
        free(path);
    // cascaded hooking
    // if (op == 'c')
    // {
    //     path = ydb_path(datablock, _new, NULL);
    //     ydb_write_hook_add(datablock, path, 0, (ydb_write_hook)notify_hook, 0);
    //     free(path);
    // }
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

    ydb_write_hook_add(datablock, "interface[name=1/2]", 0, (ydb_write_hook)notify_hook, 0);

    int n;
    for (n = 1; n <= 3; n++)
    {
        char buf[1024];
        sprintf(buf, example_yaml, n, n, n);
        res = ydb_parses(datablock, buf, strlen(buf));
        if (res)
            goto _done;
    }

    ydb_read_hook_add(datablock, "interface[name=1/1]", (ydb_read_hook)update_hook, 0);

    char enabled[32] = {0};
    ydb_read(datablock, "interface[name=1/1]: {enabled: %s}\n", enabled);
    printf("interface[name=1/1]/enabled=%s\n", enabled);

    ydb_fprintf(stdout, datablock,
                "interface[name=1/1]:\n"
                " enabled:\n"
                " mtu:\n"
                " type:\n"
                "interface[name=1/2]: {enabled}\n"
                "interface[name=1/3]:\n");
    // ydb_dump(datablock, stdout);
_done:
    if (res)
        fprintf(stderr, "%s failed. (%s)\n", __func__, ydb_res_str(res));
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

    datablock = ydb_open("top");
    if (!datablock)
    {
        fprintf(stderr, "ydb_open failed.\n");
        goto _done;
    }
        
    res = ydb_connect(datablock, NULL, "pub");
    if (res)
        goto _done;

    char path[64];
    sprintf(path, "/interfaces/interface[name=ge%d]", n);
    ydb_read_hook_add(datablock, path, (ydb_read_hook)update_hook1, 1, n);

    // ignore SIGPIPE.
    signal(SIGPIPE, SIG_IGN);
    // add a signal handler to quit this program.
    signal(SIGINT, HANDLER_SIGINT);
    do
    {
        res = ydb_serve(datablock, 10000);
        if (YDB_FAILED(res))
        {
            fprintf(stderr, "error: %s\n", ydb_res_str(res));
            goto _done;
        }
        else if (YDB_WARNING(res))
            fprintf(stdout, "warning: %s\n", ydb_res_str(res));
    } while (!done);

_done:
    if (res)
        fprintf(stderr, "%s failed. (%s)\n", __func__, ydb_res_str(res));
    ydb_close(datablock);
    // ylog_file_close();
    return res;
}

int main(int argc, char *argv[])
{
    if (argc >= 2)
    {
        ylog_level = YLOG_INFO;
        test_remote_hook(atoi(argv[1]));
    }
    else
    {
        ylog_level = YLOG_INFO;
        test_hook();
    }
    return 0;
}
