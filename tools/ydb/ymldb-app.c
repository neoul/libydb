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

// basename()
#include <libgen.h>

// getpid()
#include <sys/types.h>
#include <unistd.h>

#include "ymldb.h"

int done = 0;

void signal_handler_INT(int param)
{
    done = 1;
}


int main(int argc, char *argv[])
{
    if(argc < 2) {
        fprintf(stdout, "\n");
        fprintf(stdout, "%s MAJOR_KEY OPTIONS\n", basename(argv[0]));
        fprintf(stdout, "   -(p|sp|s) %-5s: %-24s\n", " ", "Publisher|SubPublisher|Subscriber");
        fprintf(stdout, "   -nr %-12s: %-24s\n", " ", "no_relay_to_sub_publisher mode");
        fprintf(stdout, "   -n %-12s: %-24s\n", " ", "no-record mode");
        fprintf(stdout, "   -a %-12s: %-24s\n", " ", "async mode");
        fprintf(stdout, "   -f %-12s: %-24s\n", "YMLDB_FILE", "e.g. -f ymldb_input.yml");
        fprintf(stdout, "   -v %-12s: %-24s\n", "LOG_FILE", "verbose (e.g. -v ymldb-pubisher.log or -v)");
        fprintf(stdout, "\n");
        return 0;
    }
    
    int i;
    int role = YMLDB_FLAG_SUBSCRIBER;
    int sync = 1;
    int no_record = 0;
    int no_relay_to_sub_publisher = 0;
    int verbose = 0;
    int log_level = 0;
    char *major_key = argv[1];
    char *ymldb_file = NULL;
    char *log_file = NULL;

    fprintf(stdout, "major key: %s.\n", argv[1]);
    for(i = 2; i < argc; i++)
    {
        if(strcmp(argv[i], "-f") == 0) {
            if(i+1 < argc && (strncmp(argv[i+1], "-", 1) != 0)) {
                ymldb_file = argv[i+1];
                i++;
                fprintf(stdout, "input file is %s.\n", ymldb_file);
            }
            else {
                fprintf(stdout, "no ymldb_file specified.\n");
                return 1;
            }
            
        }
        else if(strcmp(argv[i], "-p") == 0) {
            fprintf(stdout, "publisher role\n");
            role = YMLDB_FLAG_PUBLISHER;
        }
        else if(strcmp(argv[i], "-sp") == 0) {
            fprintf(stdout, "(sub)publisher role\n");
            role = YMLDB_FLAG_SUB_PUBLISHER;
        }
        else if(strcmp(argv[i], "-s") == 0) {
            fprintf(stdout, "subscriber role\n");
            role = YMLDB_FLAG_SUBSCRIBER;
        }
        else if(strcmp(argv[i], "-n") == 0) {
            no_record = 1;
            fprintf(stdout, "%s mode enabled\n", no_record?"no-record":"record");
        }
        else if(strcmp(argv[i], "-a") == 0) {
            fprintf(stdout, "%s mode enabled\n", sync?"sync":"async");
            sync = 0;
        }
        else if(strcmp(argv[i], "-nr") == 0) {
            fprintf(stdout, "no_relay_to_sub_publisher is enabled\n");
            no_relay_to_sub_publisher = 1;
        }
        else if(strcmp(argv[i], "-v") == 0) {
            verbose = 1;
            if(i+1 < argc && (strncmp(argv[i+1], "-", 1) != 0)) {
                log_file = argv[i+1];
                i++;
            }
            else {
                log_file = NULL;
            }
            fprintf(stdout, "%s mode enabled\n", verbose?"verbose":"normal");
            fprintf(stdout, "log file is %s.\n", log_file?log_file:"null");
        }
    }

    // MUST ignore SIGPIPE.
    signal(SIGPIPE, SIG_IGN);

    // add a signal handler to quit this program.
    signal(SIGINT, signal_handler_INT);

    // set ymldb log
    if(verbose)
	{
		ymldb_log_set(YMLDB_LOG_LOG, log_file);
	}

    // create ymldb for interface.
    unsigned int flags = role;
    flags |= ((sync)?YMLDB_FLAG_NONE:YMLDB_FLAG_ASYNC);
    flags |= ((no_record)?YMLDB_FLAG_NO_RECORD:YMLDB_FLAG_NONE);
    flags |= ((no_relay_to_sub_publisher)?YMLDB_FLAG_NO_RELAY_TO_SUB_PUBLISHER:YMLDB_FLAG_NONE);

    ymldb_create(major_key, flags);

    // read ymldb from a file.
	if(ymldb_file)
	{
		int infd = open(ymldb_file, O_RDONLY, 0644);
		ymldb_run_with_fd(major_key, infd, 0);
		close(infd);
	}

    ymldb_dump(stdout, NULL);

    int res;
    int max_fd = 0;
    fd_set read_set;
    struct timeval tv;
    int show_help = 1;
    do
    {
        if(show_help)
        {
            fprintf(stdout, "\n");
            fprintf(stdout, "(sync|get|write|delete|print) KEY1 KEY2 ... DATA\n");
            fprintf(stdout, "  - KEY: key to access a data.\n");
            fprintf(stdout, "  - DATA: data to write a data.\n");
            fprintf(stdout, "  * verbose (on|off|stdio|file LOG_FILE): debugging\n");
            fprintf(stdout, "  * file YMLDB_FILE: read data from a file\n");
            fprintf(stdout, "  * print: print current ymldb tree\n");
            fprintf(stdout, "> ");
            fflush(stdout);
            show_help = 0;
        }
        
        FD_ZERO(&read_set);
        tv.tv_sec = 1000;
        tv.tv_usec = 0;

        max_fd = ymldb_distribution_set(&read_set);
        FD_SET(STDIN_FILENO, &read_set);

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
                else if(strncmp(cmd, "print", 5) == 0)
                {
                    ymldb_dump(stdout, NULL);
                }
                else if(strncmp(cmd, "write", 5) == 0)
                {
                    res = ymldb_write2(cnt, keys);
                }
                else if(strncmp(cmd, "delete", 6) == 0)
                {
                    res = ymldb_delete2(cnt, keys);
                }
                else if(strncmp(cmd, "file", 6) == 0)
                {
					if(cnt >= 1)
					{
						static char _ymldb_file[32] = {0};
						strncpy(_ymldb_file, keys[1], sizeof(_ymldb_file));
						ymldb_file = _ymldb_file;
						int infd = open(ymldb_file, O_RDONLY, 0644);
						res = ymldb_run_with_fd(major_key, infd, 0);
						close(infd);
					}
					else
					{
						fprintf(stdout, "need to specify the file to read\n");
						continue;
					}
                }
                else if(strncmp(cmd, "verbose", 7) == 0)
                {
                    if(cnt >= 2) {
                        if(strcmp("off", keys[1]) == 0)
                        {
                            log_level = YMLDB_LOG_NONE;
                            log_file = NULL;
                        }
                        else if(strcmp("on", keys[1]) == 0)
                        {
                            log_level = YMLDB_LOG_LOG;
                        }
                        else if(strcmp("file", keys[1]) == 0)
                        {
                            static char _log_file[32] = {0};
                            if(cnt > 2) {
                                strncpy(_log_file, keys[2], sizeof(_log_file));
                                log_file = _log_file;
                            }
                            else
                                log_file = NULL;
                        }
                        else if(strcmp("stdio", keys[1]) == 0)
                            log_file = NULL;
                    }
                    ymldb_log_set(log_level, log_file);
                }
                if(res < 0) {
                    fprintf(stderr, "CMD failed (%d)\n", res);
                }
            }
            show_help = 1;
        }
        else
            ymldb_distribution_recv(&read_set);
    } while (!done);
    ymldb_dump(stdout, NULL);
    ymldb_destroy_all();
    fprintf(stdout, "end.\n");
    return 0;
}