#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

// time()
#include <time.h>

// mkfifo()
#include <sys/types.h>
#include <sys/stat.h>

#include "ymldb.h"

int done = 0;

void signal_handler_INT(int param)
{
    done = 1;
}


struct ymldb_cb *ymldb_test1()
{
    // write a file for a ymldb stream input.
    FILE *f = fopen("ymldb-interface.yml", "w");
    if (!f)
    {
        fprintf(stderr, "%s", strerror(errno));
        return NULL;
    }
    fputs(
        "%TAG !seq! ymldb:seq:11\n"
        "%TAG !merge! ymldb:op:merge\n"
        "---\n"
        "interface:\n"
        "  ge1:\n"
        "    mtu: 1500\n"
        "    operstatus: false\n"
        "    rx-octet: 10022\n"
        "    tx-octet: 2222\n"
        "    ip-addr: [192.168.44.1, 192.168.0.77]\n"
        "  ge2:\n"
        "    operstatus: false\n"
        "    rx-octet: 10022\n"
        "    tx-octet: 2222\n"
        "    mtu: 2000\n"
        "...\n"
        "#2 empty message\n"
        "---\n"
        "...\n",
        f);
    fclose(f);

    struct ymldb_cb *cb;

    // create ymldb for interface.
    cb = ymldb_create("interface", YMLDB_FLAG_PUBLISHER);
    if (!cb)
    {
        fprintf(stderr, "ymldb create failed.\n");
        return NULL;
    }

    // read ymldb from a file.
    int infd = open("ymldb-interface.yml", O_RDONLY, 0644);
    ymldb_run(cb, infd, 0);
    close(infd);
    return cb;
}

void ymldb_test2()
{
    struct ymldb_cb *cb = ymldb_cb("interface");

    // get data from ymldb.
    char *value = ymldb_read(cb, "interface", "ge1", "operstatus");
    fprintf(stdout, "ge1 operstatus=%s\n", value);

    // get data from ymldb using ymldb_pull.
    int mtu = 0;
    char operstatus_str[32];
    ymldb_pull(cb,
               "interface:\n"
               "  ge2:\n"
               "    operstatus: %s\n"
               "    mtu: %d\n",
               operstatus_str, &mtu);
    fprintf(stdout, "ge2 mtu=%d\n", mtu);
    fprintf(stdout, "ge2 operstatus=%s\n", operstatus_str);

    // read ymldb data (yaml format string) to OUTPUT stream.
    ymldb_get(cb, stdout, "interface", "ge2");

    mtu = 9000;
    ymldb_push(cb, 
        "interface:\n"
        "  ge2:\n"
        "    mtu: %d", mtu
        );
}

int main(int argc, char *argv[])
{
    int res;
    int cnt = 0;
    int max_fd = 0;
    fd_set read_set;
    struct timeval tv;
    time_t cur_time;
    time_t last_time;

    // MUST ignore SIGPIPE.
    signal(SIGPIPE, SIG_IGN);

    // add a signal handler to quit this program.
    signal(SIGINT, signal_handler_INT);

    struct ymldb_cb *cb = ymldb_test1();

    ymldb_dump_all(stdout);
    // fprintf(stdout, "Press any key to run.\n");
    // fgetc(stdin);

    res = mkfifo("ymldb-input", 0644);
    if(res < 0) {
        fprintf(stderr, "unable to make a named pipe (%s)\n", strerror(errno));
    }

    int local_fd = open("ymldb-input", O_RDWR, 0644);
    ymldb_conn_add(cb, local_fd);

    time(&last_time);
    do {
        FD_ZERO(&read_set);
        tv.tv_sec = 3;
        tv.tv_usec = 0;

        max_fd = ymldb_conn_set(cb, &read_set);
        res = select(max_fd + 1, &read_set, NULL, NULL, &tv);
        if(res < 0) {
            fprintf(stderr, "select failed (%s)\n", strerror(errno));
            break;
        }
        ymldb_conn_recv(cb, &read_set);

        time(&cur_time);
        double diff = difftime(cur_time, last_time);
        if(diff > 10.0) {
            cnt++;
            last_time = cur_time;
            printf("\nno. %d\n", cnt);
            ymldb_test2();
        }
    } while(!done);
    ymldb_dump_all(stdout);
    ymldb_destroy(cb);
    ymldb_dump_all(stdout);
    fprintf(stdout, "end.\n");
    return 0;
}