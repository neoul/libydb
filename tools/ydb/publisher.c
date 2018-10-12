#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

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
	ydb_connect(db, NULL, "p");
	while (res >= 0 && !done)
	{
		static int count;
		res = ydb_serve(db, 5000);
		printf("done = %d, res = %d\n", done, res);
		ydb_write(db, "count-%d: %d\n", count, count);
		count++;
	}
	ydb_close(db);
    return 0;
}
