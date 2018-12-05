#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ylog.h"
#include "ydb.h"

char *example_yaml =
    "system:\n"
    " monitor:\n"
    " stats:\n"
    "  rx-cnt: !!int 2000\n"
    "  tx-cnt: 2010\n"
    "  rmon:\n"
    "    rx-frame: 1343\n"
    "    rx-frame-64: 2343\n"
    "    rx-frame-65-127: 233\n"
    "    rx-frame-etc: 2\n"
    "    tx-frame: 2343\n"
    " mgmt:\n"
    "interfaces:\n"
    "  - eth0\n"
    "  - eth1\n"
    "  - eth3\n";

char *example_yaml2 =
    "monitor:\n"
    " mem: 10\n"
    " cpu: amd64\n";

int test_ydb_open_close()
{
    printf("\n\n=== %s ===\n", __func__);
    ydb *b1;
    b1 = ydb_open("system-info");
    ydb_write(b1, 
        "hostname: %s\n"
        "nic:\n"
        " name: eth0\n"
        " ip:\n"
        "  - 10.10.1.2\n"
        "  - 10.10.1.3\n",
        "myhost"
    );
    ydb_dump(b1, stdout);
    ydb_close(b1);
    printf("\n");
    return 0;
}

ydb_res update_hook(ydb *datablock, char *path, FILE *ydb_fp, void *U1, void *U2, void *U3)
{
    static int count;
    printf("HOOK %s path=%s U1=%p U2=%p U3=%p\n", __func__, path, U1, U2, U3);
    count++;
    fprintf(ydb_fp,
            "system:\n"
            " hostname: my-pc%d\n", count);
    return YDB_OK;
}

void notify_hook(ydb *datablock, char op, ynode *base, ynode *cur, ynode *_new, void *U1, void *U2)
{
    char *path = ydb_path(datablock, base, NULL);
    fprintf(stdout, "HOOK %s (%c) base=%s cur=%s new=%s U0=%p U1=%p U2=%p\n", __func__, op,
            path ? path: "unknown path",
           ydb_value(cur) ? ydb_value(cur) : "", ydb_value(_new) ? ydb_value(_new) : "", datablock, U1, U2);
    if (path)
        free(path);
}

int test_ydb_read_write()
{
    int num;
    ydb_res res = YDB_OK;
    printf("\n\n=== %s ===\n", __func__);
    ydb *datablock;
    datablock = ydb_open("/path/to/data");

    // inserting one value
    res = ydb_write(datablock, "VALUE");
    if (res)
        goto _done;
    char value[128] = {0};
    num = ydb_read(datablock, "%s\n", value);
    printf("num %d, value=%s\n", num, value);

    // insert a list.
    res = ydb_write(datablock,
                    "- list-entry1\n"
                    "- list-entry2\n"
                    "- list-entry3\n");
    if (res)
        goto _done;
    char entry[3][64];
    entry[0][0] = 0;
    entry[1][0] = 0;
    entry[2][0] = 0;
    num = ydb_read(datablock,
                   "- %s\n"
                   "- %s\n"
                   "- %s\n",
                   entry[0],
                   entry[1],
                   entry[2]);

    printf("num %d, list-entry=%s\n", num, entry[0]);
    printf("num %d, list-entry=%s\n", num, entry[1]);
    printf("num %d, list-entry=%s\n", num, entry[2]);

    ydb_delete(datablock, "- list-entry2\n");
    // ydb_path_delete(datablock, "/1");

    entry[0][0] = 0;
    entry[1][0] = 0;
    entry[2][0] = 0;
    num = ydb_read(datablock,
                   "- %s\n"
                   "- %s\n"
                   "- %s\n",
                   entry[0],
                   entry[1],
                   entry[2]);

    printf("num %d, list-entry=%s\n", num, entry[0]);
    printf("num %d, list-entry=%s\n", num, entry[1]);
    printf("num %d, list-entry=%s\n", num, entry[2]);

    res = ydb_write(datablock,
                    "system:\n"
                    " hostname: %s\n"
                    " fan-speed: %d\n"
                    " fan-enable: %s\n"
                    " os: linux\n"
                    "interface:\n"
                    " line1:\n"
                    "  line:\n"
                    "   line:\n"
                    "    channel:\n"
                    "     status:\n"
                    "      downstream:\n"
                    "       net-data-rate: 10\n"
                    "       expected-throughput: 20\n"
                    "       gamma-data-rate: 30\n"
                    "       attainable-net-data-rate: 40\n"
                    "      upstream:\n" 
                    "       net-data-rate: 400\n"
                    "       expected-throughput: 300\n"
                    "       gamma-data-rate: 200\n"
                    "       attainable-net-data-rate: 100\n"
                    " line2:\n"
                    "  line:\n"
                    "   line:\n"
                    "    channel:\n"
                    "     status:\n"
                    "      downstream:\n"
                    "       net-data-rate: 100\n"
                    "       expected-throughput: 2000\n"
                    "       gamma-data-rate: 3000\n"
                    "       attainable-net-data-rate: 4000\n"
                    "      upstream:\n" 
                    "       net-data-rate: 4\n"
                    "       expected-throughput: 3\n"
                    "       gamma-data-rate: 2\n"
                    "       attainable-net-data-rate: 1\n"
                    "h/w: unknown",
                    "my-machine",
                    100,
                    "True");
    if (res)
        goto _done;

    ynode *iter = NULL;
    ydb_dump(ydb_get("/path/to/data/system/hostname", &iter), stdout);
    printf("ynode = %s\n", ydb_value(iter));
    int pathlen = 0;
    char *path = ydb_path(datablock, iter, &pathlen);
    printf("ydb_path=%s\n", path);
    if (path)
        free(path);

    ydb_delete(datablock, "system: {fan-enable: , }");

    ydb_read_hook_add(datablock, "/system/hostname", (ydb_read_hook)update_hook, 3, 1, 2, 3);
    ydb_write_hook_add(datablock, "/system/hostname", 0, (ydb_write_hook)notify_hook, 2, 1, 2);

    int speed = 0;
    char hostname[128] = {
        0,
    };
    num = ydb_read(datablock,
                   "system:\n"
                   " hostname: %s\n"
                   " fan-speed: %d\n",
                   hostname, &speed);
    printf("num=%d hostname=%s, fan-speed=%d\n", num, hostname, speed);
    if (num < 0)
        goto _done;

    ydb_path_write(datablock, "system/temporature=%d", 60);
    ydb_path_write(datablock, "system/running=%s", "2 hours");

    char *temp = ydb_path_read(datablock, "system/temporature");
    printf("temporature=%d\n", atoi(temp));

    ydb_fprintf(stdout, datablock, "system: {fan-speed, hostname}\n");
    ydb_fprintf(stdout, datablock, 
        "system: {hostname}\n"
        "interface: {line2}\n"
        );

    ydb_path_fprintf(stdout, datablock, "/");
    ydb_path_fprintf(stdout, datablock, "/system");
    ydb_path_fprintf(stdout, datablock, "/system/running");

    ydb_path_delete(datablock, "system/os");

_done:
    ydb_close(datablock);
    printf("\n");
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
    ylog_severity = YLOG_DEBUG;
    TEST_FUNC(test_ydb_open_close);

    ylog_severity = YLOG_INFO;
    TEST_FUNC(test_ydb_read_write);
    return 0;
}
