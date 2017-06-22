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
    int res;
    int done = 0;
    int max_fd = 0;
    fd_set read_set;
    struct timeval tv;
    struct ymldb_cb *cb = NULL;

    cb = ymldb_create("interface", NULL);
    if(!cb) {
        return -1;
    }

    res = ymldb_fd_init(cb, YMLDB_FLAG_PUBLISHER);
    if(res < 0) {
        return -1;
    }

    for(int i=0; i<4; i++) {
        ymldb_push(cb, YMLDB_OP_MERGE,
               "interface:\n"
               "  ge%d:\n"
               "    speed: %s\n"
               "    duplex: %s",
               i+1,
               "1000",
               "full");
    }



    do {
        FD_ZERO(&read_set);
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        max_fd = ymldb_fd_set(cb, &read_set);
        res = select(max_fd + 1, &read_set, NULL, NULL, &tv);
        if(res < 0) {
            _log_error("select failed (%s)\n", strerror(errno));
            break;
        }

        ymldb_fd_run(cb, &read_set);
        sleep(1);
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