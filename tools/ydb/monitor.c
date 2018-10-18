#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/select.h>

#include "ydb.h"

static int done = 0;
void HANDLER_SIGINT(int param)
{
    done = 1;
}

void usage(int status, char *progname)
{
    if (status != 0)
        fprintf(stderr, "Try `%s --help' for more information.\n", progname);
    else
    {
        printf("Usage : %s --name=YDB_NAME or --addr=YDB_ADDR\n", progname);
        printf("\
YAML DATABLOCK monitor\n\
  -n, --name YDB_NAME   The name of created YDB (YAML DataBlock).\n\
  -a, --addr YDB_ADDR   The YAML DataBlock communication address.\n\
                        e.g. us:///tmp/ydb\n\
  -h, --help            Display this help and exit\n\
\n\
");
    }
    exit(status);
}

int main(int argc, char *argv[])
{
    int c;
    char *p;
    char *progname = ((p = strrchr(argv[0], '/')) ? ++p : argv[0]);
    char *name = NULL;
    char *addr = NULL;
    char flags[32] = {"s"};

    while (1)
    {
        int index = 0;
        static struct option long_options[] = {
            {"name", required_argument, 0, 'n'},
            {"addr", required_argument, 0, 'a'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}};

        c = getopt_long(argc, argv, "n:a:h",
                        long_options, &index);
        if (c == -1)
            break;

        switch (c)
        {
        case 'n':
            name = optarg;
            break;
        case 'a':
            addr = optarg;
            break;
        case 'h':
            usage(0, progname);
        case '?':
        default:
            usage(1, progname);
        }
    }
    if (!name && !addr)
        usage(1, progname);

    {
        ydb_res res;
        ydb *datablock;
        // ignore SIGPIPE.
        signal(SIGPIPE, SIG_IGN);
        // add a signal handler to quit this program.
        signal(SIGINT, HANDLER_SIGINT);
        ydb_log_severity = YDB_LOG_DBG;
        datablock = ydb_open(name?name:"top");
        res = ydb_connect(datablock, "file://hello.yaml", "w:");
        if (res)
        {
            fprintf(stderr, "ydb error: %s\n", ydb_res_str[res]);
            exit(EXIT_FAILURE);
        }
        res = ydb_connect(datablock, addr, flags);
        if (res)
        {
            fprintf(stderr, "ydb error: %s\n", ydb_res_str[res]);
            exit(EXIT_FAILURE);
        }


        fd_set read_set;
        struct timeval tv;

        do
        {
            int ret;
            FD_ZERO(&read_set);
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            FD_SET(ydb_fd(datablock), &read_set);
            ret = select(ydb_fd(datablock) + 1, &read_set, NULL, NULL, &tv);
            if (ret < 0)
                break;
            res = ydb_serve(datablock, 5000);
        } while (!res && !done);
        if (res)
            fprintf(stderr, "ydb error: %s\n", strerror(errno));
        fprintf(stdout, "\n# [datablock]\n");
        ydb_dump(datablock, stdout);
        ydb_close(datablock);
    }
    return 0;
}

