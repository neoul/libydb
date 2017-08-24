#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

// mkfifo()
#include <sys/types.h>
#include <sys/stat.h>

#include "ymldb.h"

int done = 0;

void signal_handler_INT(int param)
{
    done = 1;
}

int main(int argc, char *argv[])
{
    int res;
    int max_fd = 0;
    fd_set read_set;
    struct timeval tv;

    if(argc != 2) {
        fprintf(stdout, "%s [major_key]\n", argv[0]);
        return 0;
    }

    // MUST ignore SIGPIPE.
    signal(SIGPIPE, SIG_IGN);

    // add a signal handler to quit this program.
    signal(SIGINT, signal_handler_INT);

    ymldb_log_set(YMLDB_LOG_LOG, "/tmp/ymldb-subscriber.log");

    // create ymldb for interface.
    ymldb_create(argv[1], YMLDB_FLAG_NONE);
    ymldb_distribution_init(argv[1], YMLDB_FLAG_SUBSCRIBER);
    // ymldb_distribution_add(argv[1], STDOUT_FILENO);

    do
    {
        fprintf(stdout, "\n");
        fprintf(stdout, "CMD KEY1 KEY2 ... DATA\n");
        fprintf(stdout, "  CMD: (sync|get|write|delete)\n");
        fprintf(stdout, "  KEY: key to access a data.\n");
        fprintf(stdout, "  DATA: data to write a data.\n");
        FD_ZERO(&read_set);
        tv.tv_sec = 1000;
        tv.tv_usec = 0;

        max_fd = ymldb_distribution_set(&read_set);
        FD_SET(STDIN_FILENO, &read_set);
        fprintf(stdout, "> ");
        fflush(stdout);
        res = select(max_fd + 1, &read_set, NULL, NULL, &tv);
        if (res < 0)
        {
            fprintf(stderr, "select failed (%s)\n", strerror(errno));
            break;
        }
        if(FD_ISSET(STDIN_FILENO, &read_set))
        {
            int cnt = 1;
            char *cmd;
            char *pch;
            char buf[128];
            char *keys[10];
            pch = fgets(buf, sizeof(buf), stdin);
            keys[0] = argv[1];
            cmd = strtok (buf," \n\t");
            if(cmd) {
                while (cnt < 10)
                {
                    pch = strtok (NULL, " \n\t");
                    if(!pch) break;
                    fprintf (stdout, "key=%s\n",pch);
                    keys[cnt] = pch;
                    cnt++;
                }

                res = 0;
                if(strncmp(cmd, "sync", 4) == 0)
                {
                    fprintf(stdout, "SYNC!\n");
                    res = ymldb_sync2(cnt, keys);
                }
                else if(strncmp(cmd, "get", 3) == 0)
                {
                    res = ymldb_get2(stdout, cnt, keys);
                }
                else if(strncmp(cmd, "write", 5) == 0)
                {
                    res = ymldb_write2(cnt, keys);
                }
                else if(strncmp(cmd, "delete", 6) == 0)
                {
                    res = ymldb_delete2(cnt, keys);
                }
                if(res < 0) {
                    fprintf(stderr, "CMD failed (%d)\n", res);
                }
            }
        }
        ymldb_distribution_recv(&read_set);
    } while (!done);
    ymldb_dump_all(stdout, NULL);
    ymldb_destroy_all();
    ymldb_dump_all(stdout, NULL);
    fprintf(stdout, "end.\n");
    return 0;
}