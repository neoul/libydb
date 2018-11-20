#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "ylog.h"
#include "ydb.h"

char *example_yaml =
    "mgmt:\n"
    " description: \n"
    " enabled: true\n"
    " ipv4:\n"
    "  address:\n"
    "   192.168.77.10:\n"
    "    prefix-length: 16\n"
    "    secondary: false\n"
    " link-up-down-trap-enable: disabled\n"
    " name: mgmt\n"
    " type: mgmt\n"
    "xe1:\n"
    " description: \n"
    " enabled: false\n"
    " ethernet:\n"
    "  auto-negotiation:\n"
    "   enable: true\n"
    "  duplex: full\n"
    "  speed: 10.000\n"
    " link-up-down-trap-enable: disabled\n"
    " name: xe1\n"
    " type: ethernetCsmacd\n"
    "xe2:\n"
    " description: \n"
    " enabled: true\n"
    " ethernet:\n"
    "  auto-negotiation:\n"
    "   enable: false\n"
    "  duplex: full\n"
    "  speed: 10.000\n"
    " link-up-down-trap-enable: disabled\n"
    " name: xe2\n"
    " type: ethernetCsmacd\n";

ydb_res update_hook(ydb *datablock, char *path, FILE *fp)
{
    static int enabled;
    printf("HOOK %s path=%s\n", __func__, path);
    enabled = (enabled + 1) % 2;
    fprintf(fp,
            "xe1:\n"
            " enabled: %s\n",
            enabled ? "true" : "false");
    return YDB_OK;
}

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

    res = ydb_parses(datablock, example_yaml, strlen(example_yaml));
    if (res)
        goto _done;

    ydb_read_hook_add(datablock, "/xe1/enabled", (ydb_read_hook)update_hook, 0);
    ydb_write_hook_add(datablock, "/mgmt", (ydb_write_hook)notify_hook, NULL, 0);

    char enabled[32] = {0};
    ydb_read(datablock, "xe1: {enabled: %s}\n", enabled);
    printf("/xe1/enabled=%s\n", enabled);

    ydb_fprintf(stdout, datablock,
                "xe1: {enabled}\n"
                "xe2: {enabled}\n"
                "mgmt:\n");
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

int test_remote_hook()
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

    res = ydb_connect(datablock, NULL, "pub:sync");
    if (res)
        goto _done;

    res = ydb_parses(datablock, example_yaml, strlen(example_yaml));
    if (res)
        goto _done;

    ydb_read_hook_add(datablock, "/xe1/enabled", (ydb_read_hook)update_hook, 0);
    ydb_write_hook_add(datablock, "/mgmt", (ydb_write_hook)notify_hook, NULL, 0);

    // ignore SIGPIPE.
    signal(SIGPIPE, SIG_IGN);
    // add a signal handler to quit this program.
    signal(SIGINT, HANDLER_SIGINT);
    do
    {
        res = ydb_serve(datablock, 5000);
        if (res)
            goto _done;
    } while (!done);

_done:
    if (res)
        fprintf(stderr, "%s failed. (%s)\n", __func__, ydb_res_str[res]);
    ydb_close(datablock);
    return res;
}

#define TEST_FUNC(func)                    \
    do                                     \
    {                                      \
        if (func())                        \
        {                                  \
            printf("%s failed.\n", #func); \
            return -1;                     \
        }                                  \
    } while (0)

int main(int argc, char *argv[])
{
    if (argc >= 2)
    {
        ylog_severity = YLOG_DEBUG;
        TEST_FUNC(test_remote_hook);
    }
    else
    {
        ylog_severity = YLOG_DEBUG;
        TEST_FUNC(test_hook);
    }
    return 0;
}
