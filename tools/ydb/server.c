#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "ydb.h"

int done = 0;
void HANDLER_SIGINT(int param)
{
    done = 1;
}

int main(int argc, char *argv[])
{
	ydb_res res = YDB_OK;
    // MUST ignore SIGPIPE.
    // signal(SIGPIPE, SIG_IGN);
    // add a signal handler to quit this program.
    signal(SIGINT, HANDLER_SIGINT);

	ydb_log_severity = YDB_LOG_DBG;

	ydb *db;
	db = ydb_open("/system/ipc", NULL, "s");
	while (!res)
		res = ydb_serve(db, 1000);
	ydb_close(db);
    return 0;
}
