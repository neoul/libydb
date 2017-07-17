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

    ymldb_log_set(YMLDB_LOG_LOG, NULL);

    // create ymldb for interface.
    ymldb_create("interface", YMLDB_FLAG_PUBLISHER);
    // read ymldb from a file.
    int infd = open("ymldb-interface.yml", O_RDONLY, 0644);
    ymldb_run_with_fd("interface", infd, 0);
    close(infd);

    ymldb_dump_all(stdout);

    // unlink("ymldb-io");
    // mkfifo("ymldb-io", 0644);

    // int local_fd = open("ymldb-io", O_WRONLY, 0644);
    // ymldb_distribution_add("interface", local_fd);

    time(&last_time);
    do
    {
        FD_ZERO(&read_set);
        tv.tv_sec = 3;
        tv.tv_usec = 0;

        max_fd = ymldb_distribution_set(&read_set);
        res = select(max_fd + 1, &read_set, NULL, NULL, &tv);
        if (res < 0)
        {
            fprintf(stderr, "select failed (%s)\n", strerror(errno));
            break;
        }
        int fd_publisher = 0;
        int *fd_subscriber = NULL;
        int fd_subscriber_count = 0;
        ymldb_distribution_get_publisher_fd("interface", &fd_publisher);
        ymldb_distribution_get_subscriber_fds("interface", &fd_subscriber, &fd_subscriber_count);
        // printf("publisher_fd %d\n", fd_publisher);
        // int i;
        // for(i = 0; i < fd_subscriber_count; i++) {
        //     printf("subscriber_fd[%d] %d\n", i, fd_subscriber[i]);
        // }
        

        ymldb_distribution_recv(&read_set);

        time(&cur_time);
        double diff = difftime(cur_time, last_time);
        if (diff > 10.0)
        {
            char ifname[32] = {0};
            char mtu[32] = {0};
            sprintf(ifname, "ge%d", cnt/3);
            sprintf(mtu, "%d", cnt * 100);
            
            printf("\nno. %d\n", cnt);
            if(cnt%3 == 0)
                ymldb_push("interface",
                        "interface:\n"
                        "  ge%d:\n"
                        "    mtu: %s",
                        cnt/3,
                        mtu);
            if(cnt%3 == 1)
                ymldb_write("interface",  ifname, "mtu", mtu);
            if(cnt%3 == 2)
                ymldb_delete("interface",  ifname);
            last_time = cur_time;
            cnt++;
        }
    } while (!done);
    ymldb_dump_all(stdout);
    ymldb_destroy_all();
    ymldb_dump_all(stdout);
    fprintf(stdout, "end.\n");
    return 0;
}