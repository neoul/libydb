#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

#include "ylog.h"
#include "ydb.h"

static int done;
void HANDLER_SIGINT(int signal)
{
    done = 1;
}

int main(int argc, char *argv[])
{
    int fd, ret;
    fd_set read_set;
    struct timeval tv;

    ydb_res res;
    ydb *datablock;

    if (argc < 2)
    {
        fprintf(stdout,
                " [%s]\n"
                "   ydb + select() example\n"
                "   ydb_server() retry to open the disconnected communication.\n\n",
                argv[0]);
        fprintf(stdout, " [usage]\n");
        fprintf(stdout, "   %s ADDR1 (ADDR2 ...)\n\n", argv[0]);
        fprintf(stdout, " [example]\n");
        fprintf(stdout, "   %s uss://top uss://hello\n", argv[0]);
        return 0;
    }

    res = YDB_OK;
    // ylog_severity = YLOG_INFO;
    ylog_severity = YLOG_DEBUG;
    datablock = ydb_open("select");
    if (!datablock)
    {
        fprintf(stderr, "error: ydb open\n");
        return 1;
    }

    int n = 1;
    for (; n < argc; n++)
    {
        res = ydb_connect(datablock, argv[n], "sub");
        if (YDB_FAILED(res))
            goto failed;
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, HANDLER_SIGINT);

    while (!done)
    {
        fd = ydb_fd(datablock);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        FD_ZERO(&read_set);
        if (fd < 0)
        {
            res = YDB_E_NO_CONN;
            goto failed;
        }
        FD_SET(fd, &read_set);
        ret = select(fd + 1, &read_set, NULL, NULL, &tv);
        if (ret < 0)
        {
            done = 1;
        }
        else if (ret == 0)
        {
            // timeout
        }
        else
        {
            if (FD_ISSET(fd, &read_set))
            {
                FD_CLR(fd, &read_set);
                // ydb_serve() called on which the timer expired or fd event set.
                res = ydb_serve(datablock, 0);
                if (YDB_FAILED(res))
                {
                    fprintf(stderr, "error: %s\n", ydb_res_str(res));
                    goto failed;
                }
                else if (YDB_WARNING(res))
                    fprintf(stdout, "warning: %s\n", ydb_res_str(res));
            }
        }
    }
failed:
    ydb_close(datablock);
    if (YDB_FAILED(res))
        fprintf(stderr, "error: %s\n", ydb_res_str(res));
    else if (YDB_WARNING(res))
        fprintf(stdout, "warning: %s\n", ydb_res_str(res));
    return 0;
}