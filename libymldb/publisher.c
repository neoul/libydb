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

    // MUST ignore SIGPIPE.
    signal(SIGPIPE, SIG_IGN);

    cb = ymldb_create("interface", YMLDB_FLAG_PUBLISHER|YMLDB_FLAG_NOSYNC);
    if(!cb) {
        return -1;
    }

    int infd = open("ymldb-interface.yml", O_RDONLY, 0644);
    if (infd < 0)
    {
        fprintf(stderr, "file open error. %s\n", strerror(errno));
        return 1;
    }

    ymldb_run(cb, infd, 0);
    ymldb_dump_all(stdout);

    do {
        cnt++;
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

        // ymldb_push(cb, 
        //     "interface:\n"
        //     "  ge%d:\n"
        //     "    speed: %d\n"
        //     "    duplex: %s",
        //     cnt+1,
        //     cnt+1000,
        //     "full");
        if(cnt > 20) break;
    } while(!done);
    ymldb_destroy(cb);
    ymldb_dump_all(stdout);
    return 0;
}