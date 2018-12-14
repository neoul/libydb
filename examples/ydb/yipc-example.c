#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "ylog.h"
#include "yipc.h"

static int done;
void HANDLER_SIGINT(int param)
{
	done = 1;
}

int main(int argc, char *argv[])
{
	char *src, *dest;
	int ipcfd, ret, send_interval = 0;
	struct timespec sent, cur;
	static int send_count;

	if (argc <= 3)
	{
		fprintf(stdout, " %s SRC_ID DEST_ID COUNT_AT\n", argv[0]);
		return 1;
	}

	// ylog_severity = YLOG_INFO;
	// ylog_severity = YLOG_DEBUG;
	src = argv[1];
	dest = argv[2];
	send_count = atoi(argv[3]);

	ipcfd = yipc_create(src, "uss://yipc.test");
	if (ipcfd < 0)
	{
		fprintf(stderr, "yipc_create() failed.\n");
		return 1;
	}

	// ignore SIGPIPE.
	signal(SIGPIPE, SIG_IGN);
	// add a signal handler to quit this program.
	signal(SIGINT, HANDLER_SIGINT);

	clock_gettime(CLOCK_MONOTONIC, &cur);
	clock_gettime(CLOCK_MONOTONIC, &sent);

	do
	{
		int count = 0;
		char sender[64] = {0};
		ydb *datablock = NULL;
		ret = yipc_recv(src, 2000, &datablock);
		if (ret < 0)
		{
			fprintf(stderr, "yipc_recv() failed.\n");
			break;
		}
		if (datablock)
		{
			// read who sends it.
			ydb_read(datablock, "message: {src: %s}\n", sender);
			// read what you want to read.
			ydb_read(datablock,
					 "foo:\n"
					 " bar: %d\n",
					 &count);
			fprintf(stdout, "RECV %d from %s\n", count, sender);
		}
		clock_gettime(CLOCK_MONOTONIC, &cur);
		send_interval = (cur.tv_sec - sent.tv_sec) * 1000;
		send_interval = send_interval + (cur.tv_nsec - sent.tv_nsec) / 10e5;
		if (send_interval < 0 || send_interval > YDB_TIMEOUT)
		{
			send_count++;
			printf("SEND %d to %s\n", send_count, dest);
			yipc_send(src, dest,
						"foo:\n"
						" bar: %d\n",
						send_count);
			clock_gettime(CLOCK_MONOTONIC, &sent);
		}
	} while (!done);
	yipc_destroy(src);
	return 0;
}
