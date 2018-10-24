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
    char flags[32] = {"subscriber"};

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
        ydb_log_severity = YDB_LOG_CRI;
        datablock = ydb_open(name ? name : "top");
        res = ydb_connect(datablock, "file://stdout", "w:");
        if (res)
        {
            fprintf(stderr, "ydb error: %s\n", ydb_res_str[res]);
            ydb_close(datablock);
            exit(EXIT_FAILURE);
        }
        res = ydb_connect(datablock, addr, flags);
        if (res)
        {
            fprintf(stderr, "ydb error: %s\n", ydb_res_str[res]);
            ydb_close(datablock);
            exit(EXIT_FAILURE);
        }

        fd_set read_set;
        struct timeval tv;

        do
        {
            int ret;
            int fd = ydb_fd(datablock);
            if (fd < 0)
            {
                fprintf(stderr, "ydb error: %s\n", ydb_res_str[YDB_E_NO_CONN]);
                break;
            }
            FD_ZERO(&read_set);
            tv.tv_sec = 5;
            tv.tv_usec = 0;

            FD_SET(fd, &read_set);
            ret = select(fd + 1, &read_set, NULL, NULL, &tv);
            if (ret < 0)
                break;
            if (FD_ISSET(fd, &read_set))
            {
                res = ydb_serve(datablock, 0);
                if (res)
                {
                    fprintf(stderr, "ydb error: %s\n", ydb_res_str[res]);
                    break;
                }
            }
        } while (!res && !done);
        
        fprintf(stdout, "\n# [datablock]\n");
        ydb_dump(datablock, stdout);
        ydb_close(datablock);
    }
    return 0;
}
