#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include "ymldb.h"

int main(int argc, char *argv[])
{
    int res;
    int cnt = 0;
    int done = 0;
    int max_fd = 0;
    fd_set read_set;
    struct timeval tv;
    struct ymldb_cb *cb = NULL;
    signal(SIGPIPE, SIG_IGN);

    int outfd = STDOUT_FILENO;
    if (strlen("subscriber.result.yml") > 0)
    {
        outfd = open("subscriber.result.yml", O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (outfd < 0)
        {
            fprintf(stderr, "subscriber.result.yml file open error. %s\n", strerror(errno));
            return 1;
        }
    }

    cb = ymldb_create("interface", YMLDB_FLAG_SUBSCRIBER | YMLDB_FLAG_LOCAL, outfd);
    if(!cb) {
        return -1;
    }

    do {
        cnt ++;
        printf("\n\nno. %d\n", cnt);
        FD_ZERO(&read_set);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        max_fd = ymldb_conn_set(cb, &read_set);
        res = select(max_fd + 1, &read_set, NULL, NULL, &tv);
        if(res < 0) {
            _log_error("select failed (%s)\n", strerror(errno));
            break;
        }
        ymldb_conn_recv(cb, &read_set);
        if(cnt > 5) break;
    } while(!done);
    ymldb_dump(stdout, cb->ydb, 0, 0);
    ymldb_destroy(cb);
    print_alloc_cnt();
    close(outfd);
    return 0;
}