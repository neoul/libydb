#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "ymldb.h"

int main(int argc, char *argv[])
{
    int fd;
    int res;
    int i = 0;
    int done = 0;
    int max_fd = 0;
    fd_set read_set;
    struct timeval tv;
    struct ymldb_cb *cb = NULL;

    cb = ymldb_create("interface", NULL);
    if(!cb) {
        return -1;
    }

    fd = ymldb_fd_init(cb, 0);
    if(fd < 0) {
        return -1;
    }

    do {
        FD_ZERO(&read_set);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        char writebuf[64];
        i++;
        sprintf(writebuf, 
            "interface:\n"
            "  ge%d:\n"
            "    speed: %s\n"
            "    duplex: %s",
            i+1,
            "1000",
            "full");
        write(cb->publisher, writebuf, strlen(writebuf));
        max_fd = ymldb_fd_set(cb, &read_set);
        res = select(max_fd + 1, &read_set, NULL, NULL, &tv);
        if(res < 0) {
            _log_error("select failed (%s)\n", strerror(errno));
            break;
        }

        ymldb_fd_run(cb, &read_set);
        _log_debug("timeout test...\n");
        
    } while(!done);
    

    // ymldb_run(cb, infp);

    // ymldb_push(cb, YMLDB_OP_MERGE,
    //            "system:\n"
    //            "  product: %s\n"
    //            "  serial-number: %s\n",
    //            "G.FAST-HN5124D",
    //            "HN5124-S100213124");
    // ymldb_write(cb, 3, "system", "product", "abc");

    // ymldb_dump(cb, gYdb, 0, 0);

    // char productstr[32];
    // char serial_number[32];
    // ymldb_pull(cb,
    //            "system:\n"
    //            "  serial-number: %s\n"
    //            "  product: %s\n",
    //            productstr,
    //            serial_number);

    // ymldb_destroy(cb);

    // for debug
    print_alloc_cnt();
    return 0;
}