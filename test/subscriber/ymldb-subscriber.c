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

void ymldb_notify_callback(void *usr_data, struct ymldb_callback_data *cdata)
{
    int i;
    printf("\n");
    if(cdata->deleted || cdata->unregistered)
        printf(" [callback for%s%s]\n", 
            cdata->deleted?" del":"",
            cdata->unregistered?" unreg":"");

    if(!cdata->unregistered && !cdata->deleted)
        printf(" [callback for merge]\n");
    
    printf("\t- %s(%s)\n", __FUNCTION__, (char *) (usr_data?usr_data:""));
    
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
}

int main(int argc, char *argv[])
{
    int res;
    int sync = 1;
    int max_fd = 0;
    fd_set read_set;
    struct timeval tv;

    if(argc != 2 && argc != 3) {
        fprintf(stdout, "%s [major_key]\n", argv[0]);
        fprintf(stdout, "%s [major_key] async\n", argv[0]);
        return 0;
    }
    if(argc == 3 && strncmp(argv[2], "async", 5) == 0)
    {
        printf("async mode\n");
        sync = 0;
    }

    // MUST ignore SIGPIPE.
    signal(SIGPIPE, SIG_IGN);

    // add a signal handler to quit this program.
    signal(SIGINT, signal_handler_INT);

    // set ymldb log
	{
		char logfile[64];
		pid_t pid = getpid();
		sprintf(logfile, "/tmp/ymldb-subscriber-%d.log", pid);
		ymldb_log_set(YMLDB_LOG_LOG, logfile);
		// ymldb_log_set(YMLDB_LOG_LOG, NULL); //stdout
	}

    // create ymldb for interface.
    ymldb_create(argv[1], YMLDB_FLAG_NONE);
    ymldb_distribution_init(argv[1], (YMLDB_FLAG_SUBSCRIBER | ((sync)?YMLDB_FLAG_NONE:YMLDB_FLAG_ASYNC)));
    // ymldb_distribution_add(argv[1], STDOUT_FILENO);

    if(strcmp(argv[1], "interfaces") == 0)
        ymldb_notify_callback_register(ymldb_notify_callback, 
            "interface-cb", "interfaces", "interface");

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