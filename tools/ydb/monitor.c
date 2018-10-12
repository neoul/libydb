#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>

// mkfifo()
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>

#include "ydb.h"

int done = 0;
void HANDLER_SIGINT(int param)
{
	done = 1;
	printf("set done\n");
}

int main(int argc, char *argv[])
{
	ydb *db;
	int res = 0;
	// MUST ignore SIGPIPE.
	signal(SIGPIPE, SIG_IGN);
	// add a signal handler to quit this program.
	signal(SIGINT, HANDLER_SIGINT);

	ydb_log_severity = YDB_LOG_DBG;
	db = ydb_open("/system/ipc");
	if (!db)
	{
		printf("ydb open failed\n");
		exit(EXIT_FAILURE);
	}
	ydb_connect(db, NULL, "s");

	fd_set read_set;
	struct timeval tv;

	do
	{
		FD_ZERO(&read_set);
		tv.tv_sec = 10;
		tv.tv_usec = 0;
		FD_SET(ydb_fd(db), &read_set);
		res = select(ydb_fd(db) + 1, &read_set, NULL, NULL, &tv);
		if (res < 0)
		{
			fprintf(stderr, "select failed (%s)\n", strerror(errno));
			break;
		}
		res = ydb_serve(db, 0);
		printf("done = %d, res = %d\n", done, res);
	} while (!done);

	ydb_close(db);
	return 0;
}
