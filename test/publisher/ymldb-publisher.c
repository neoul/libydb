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

void ymldb_update_callback(void *usr_data, struct ymldb_callback_data *cdata)
{
    int i;
    static int index = 0;
    static int mtu[] = {256, 512, 1024, 1518};
    static int rx_octet[] = {1000, 2000, 3000, 4000};
    static int tx_octet[] = {5000, 6000, 7000, 8000};

    printf("\n");
    printf("\t- KEYS(1):");
    for(i=0; i<cdata->keys_num; i++) {
        printf(" %s", cdata->keys[i]);
    }
    if(cdata->value) {
        printf(" = %s", cdata->value);
    }
    printf("\n");

    printf("\t- KEYS(2):");
    for(i=cdata->keys_level; i< cdata->keys_num; i++) {
        printf(" %s", cdata->keys[i]);
    }
    if(cdata->value) {
        printf(" = %s", cdata->value);
    }
    printf("\n");

    printf("\t- keys_num=%d, keys_level=%d\n\n",cdata->keys_num, cdata->keys_level);

    ymldb_push("interfaces",
        "interface:\n"
        "  ge3:\n"
        "    rx-octet: %d\n"
        "    tx-octet: %d\n"
        "    mtu: %d\n"
        ,
        rx_octet[index],
        tx_octet[index],
        mtu[index]
    );
    index = (index + 1)%4;
}

int main(int argc, char *argv[])
{
    int res;
    int sync = 1;
    int max_fd = 0;
    fd_set read_set;
    struct timeval tv;

    if(argc != 3 && argc != 4) {
        fprintf(stdout, "\n");
        fprintf(stdout, "%s [major_key] [ymldb_file.yml]\n", argv[0]);
        fprintf(stdout, "%s [major_key] [ymldb_file.yml] no-sync\n", argv[0]);
        fprintf(stdout, "\n");
        return 0;
    }
    if(argc == 4 && strncmp(argv[3], "no-sync", 7) == 0)
    {
        printf("no-sync mode\n");
        sync = 0;
    }

    // MUST ignore SIGPIPE.
    signal(SIGPIPE, SIG_IGN);

    // add a signal handler to quit this program.
    signal(SIGINT, signal_handler_INT);

    ymldb_log_set(YMLDB_LOG_LOG, "/tmp/ymldb-publisher.log");

    // create ymldb for interface.
    ymldb_create(argv[1], (YMLDB_FLAG_PUBLISHER | ((sync)?YMLDB_FLAG_NONE:YMLDB_FLAG_ASYNC)));
    
    if(strcmp(argv[1], "interfaces") == 0)
    {
        ymldb_update_callback_register(ymldb_update_callback, "USR-DATA", argv[1], "interface", "ge3");
    }

    // read ymldb from a file.
    int infd = open(argv[2], O_RDONLY, 0644);
    ymldb_run_with_fd(argv[1], infd, 0);
    close(infd);

    ymldb_dump_all(stdout, NULL);

    // unlink("ymldb-io");
    // mkfifo("ymldb-io", 0644);

    // int local_fd = open("ymldb-io", O_WRONLY, 0644);
    // ymldb_distribution_add("interface", local_fd);
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