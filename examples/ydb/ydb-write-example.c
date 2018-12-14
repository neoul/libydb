#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "ylog.h"
#include "ydb.h"

char *example_yaml =
    "interface:\n"
    " line%d:\n"
    "  line:\n"
    "   line:\n"
    "    channel:\n"
    "     status:\n"
    "      downstream:\n"
    "       net-data-rate: %u\n"
    "       expected-throughput: %u\n"
    "       gamma-data-rate: %u\n"
    "       attainable-net-data-rate: %u\n"
    "      upstream:\n" 
    "       net-data-rate: %u\n"
    "       expected-throughput: %u\n"
    "       gamma-data-rate: %u\n"
    "       attainable-net-data-rate: %u\n";

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

    ylog_file_open("ydb-write-example.log.fifo");
    datablock = ydb_open("top");
    if (!datablock)
    {
        fprintf(stderr, "ydb_open failed.\n");
        goto _done;
    }

    char buf[1024];
    sprintf(buf, example_yaml, n, n, n, n, n, n, n, n, n);
    res = ydb_parses(datablock, buf, strlen(buf));
    if (res)
        goto _done;
        
    res = ydb_connect(datablock, NULL, "pub:sync");
    if (res)
        goto _done;

    // ignore SIGPIPE.
    signal(SIGPIPE, SIG_IGN);
    // add a signal handler to quit this program.
    signal(SIGINT, HANDLER_SIGINT);
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    do
    {
        static int c;
        int timeout;
        clock_gettime(CLOCK_MONOTONIC, &end);
        res = ydb_serve(datablock, 1000);
        if (YDB_FAILED(res))
            goto _done;
        timeout = (end.tv_sec - start.tv_sec) * 1000;
        timeout = timeout + (end.tv_nsec - start.tv_nsec) / 10e5;
        if (timeout > 5000 || timeout < 0)
        {
            c++;
            ydb_write(datablock, example_yaml, n,c,c,c,c,c,c,c,c);
            clock_gettime(CLOCK_MONOTONIC, &start);
        }
    } while (!done);

_done:
    if (res)
        fprintf(stderr, "%s failed. (%s)\n", __func__, ydb_res_str(res));
    ydb_close(datablock);
    ylog_file_close();
    return res;
}

int main(int argc, char *argv[])
{
    if (argc >= 2)
    {
        ylog_severity = YLOG_DEBUG;
        test_remote_hook(atoi(argv[1]));
    }
    return 0;
}
