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
    int k;
	int no_record = 0;

    // MUST ignore SIGPIPE.
    signal(SIGPIPE, SIG_IGN);

    // add a signal handler to quit this program.
    signal(SIGINT, signal_handler_INT);
	
	// set ymldb log
	ymldb_log_set(YMLDB_LOG_LOG, "/tmp/ymldb-monitor.log");
	// ymldb_log_set(YMLDB_LOG_LOG, NULL);

    /* Analyze command line options. */
    for (k = 1; k < argc; k ++)
    {
        if (strcmp(argv[k], "-h") == 0
                || strcmp(argv[k], "--help") == 0) {
			printf("\n%s -in -out -n key1 key2 ...\n"
				   "\t-in: ymldb input monitoring\n"
				   "\t-out: ymldb output monitoring\n"
				   "\t-n: no-record\n"
				   "\tkey: YMLDB key to monitor\n\n",
					argv[0]);
			return 0;
		}

		if(strcmp(argv[k], "-in") == 0)
		{
			instream_mointor = stdout;
			continue;
		}

		if(strcmp(argv[k], "-out") == 0)
		{
			outstream_monitor = stdout;
			continue;
		}

		if(strcmp(argv[k], "-n") == 0)
		{
			no_record = 1;
			continue;
		}
		ymldb_create(argv[k], (YMLDB_FLAG_SUBSCRIBER | ((no_record)?YMLDB_FLAG_NO_RECORD:0x0)));
		no_record = 0;
	}
	if(k == 1)
	{
		printf("\n%s -in -out -n key1 key2 ...\n"
				   "\t-in: ymldb input monitoring\n"
				   "\t-out: ymldb output monitoring\n"
				   "\t-n: no-record\n"
				   "\tkey: YMLDB key to monitor\n\n",
					argv[0]);
		return 0;
	}
	// ymldb_dump(stdout, NULL);

	int res;
	int max_fd = 0;
	fd_set read_set;
	struct timeval tv;

	do
	{
		FD_ZERO(&read_set);
		tv.tv_sec = 10;
		tv.tv_usec = 0;

		max_fd = ymldb_distribution_set(&read_set);
		res = select(max_fd + 1, &read_set, NULL, NULL, &tv);
		if (res < 0)
		{
			fprintf(stderr, "select failed (%s)\n", strerror(errno));
			break;
		}
		ymldb_distribution_recv_and_dump(NULL, &read_set);
    } while (!done);
    ymldb_dump(stdout, NULL);
	ymldb_destroy_all();
    fprintf(stdout, "end.\n");
    return 0;
}
