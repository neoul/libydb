#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>

#include "ydb.h"

static int done = 1;
void HANDLER_SIGINT(int param)
{
	done = 1;
	printf("set done\n");
}

void usage(int status, char *progname)
{
	if (status != 0)
		fprintf(stderr, "Try `%s --help' for more information.\n", progname);
	else
	{
		printf("Usage : %s --name=YDB_NAME [OPTION...]\n", progname);
		printf("\
YAML DATABLOCK subscriber\n\
  -n, --name=YDB_NAME   The name of created YDB (YAML DataBlock).\n\
  -w, --write=FILE      Write and send yaml data in FILE to publisher\n\
  -u, --unsubscribe     Disable subscription\n\
  -d, --daemon          Runs in daemon mode\n\
    , --verbose         Verbose mode for debug (debug|inout|info)\n\
  -h, --help            Display this help and exit\n\
\n\
");
	}
	exit(status);
}

int main(int argc, char *argv[])
{
	int c;
	char *p;
	char *progname = ((p = strrchr(argv[0], '/')) ? ++p : argv[0]);
	char *name = NULL;
	char *wfile = NULL;
	int verbose = 0;
	char flags[32] = {"s"};

	while (1)
	{
		int index = 0;
		static struct option long_options[] = {
			{"name", required_argument, 0, 'n'},
			{"write", required_argument, 0, 'w'},
			{"verbose", required_argument, 0, 'v'},
			{"unsubscribe", no_argument, 0, 'u'},
			{"daemon", no_argument, 0, 'd'},
			{"help", no_argument, 0, 'h'},
			{0, 0, 0, 0}};

		c = getopt_long(argc, argv, "n:w:v:udh",
						long_options, &index);
		if (c == -1)
			break;

		switch (c)
		{
		case 'n':
			name = optarg;
			break;
		case 'w':
			wfile = optarg;
			break;
		case 'v':
			if (strcmp(optarg, "debug") == 0)
				verbose = YDB_LOG_DBG;
			else if (strcmp(optarg, "inout") == 0)
				verbose = YDB_LOG_INOUT;
			else if (strcmp(optarg, "info") == 0)
				verbose = YDB_LOG_INFO;
			break;
		case 'u':
			strcat(flags, ":unsubscribe");
			break;
		case 'd':
			done = 0;
			break;
		case 'h':
			usage (0, progname);
		case '?':
		default:
			usage (1, progname);
		}
	}
	if (!name)
		usage(1, progname);

	{
		ydb *db;
		int fd = 0;
		// ignore SIGPIPE.
		signal(SIGPIPE, SIG_IGN);
		// add a signal handler to quit this program.
		signal(SIGINT, HANDLER_SIGINT);
		
		if (verbose)
			ydb_log_severity = verbose;
		
		db = ydb_open(name, NULL, flags);
		if (wfile)
		{
			ynode *n = NULL;
			FILE *fp = fopen(wfile, "r");
			ydb_res res = ynode_scanf_from_fp(fp, &n);
			if (res)
			{
				fprintf(stderr, "fail to get data from %s\n", wfile);
				exit(EXIT_FAILURE);
			}
			if (fp)
				fclose(fp);
			if (n)
				ynode_remove(n);
		}

		do
		{
			fd = ydb_serve(db, 5000);
			printf("done = %d, fd = %d\n", done, fd);
		} while (fd >= 0 && !done);
		ydb_close(db);
	}
	return 0;
}
