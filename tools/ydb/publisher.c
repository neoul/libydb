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
YAML DATABLOCK publisher\n\
  -n, --name YDB_NAME   The name of created YDB (YAML DataBlock).\n\
  -r, --file FILE       Read data from FILE to send publisher\n\
  -u, --unsubscribe     Disable subscription\n\
  -r, --reconnect       Retry to reconnect upon the communication failure\n\
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
    char *rfile = NULL;
    int verbose = 0;
    char flags[32] = {"p"};

    while (1)
    {
        int index = 0;
        static struct option long_options[] = {
            {"name", required_argument, 0, 'n'},
            {"file", required_argument, 0, 'f'},
            {"verbose", required_argument, 0, 'v'},
            {"unsubscribe", no_argument, 0, 'u'},
            {"reconnect", no_argument, 0, 'r'},
            {"daemon", no_argument, 0, 'd'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}};

        c = getopt_long(argc, argv, "n:w:v:urdh",
                        long_options, &index);
        if (c == -1)
            break;

        switch (c)
        {
        case 'n':
            name = optarg;
            break;
        case 'f':
            rfile = optarg;
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
            strcat(flags, ":u");
            break;
        case 'r':
            strcat(flags, ":r");
            break;
        case 'd':
            done = 0;
            break;
        case 'h':
            usage(0, progname);
        case '?':
        default:
            usage(1, progname);
        }
    }
    if (!name)
        usage(1, progname);
    printf("configured options:\n");
    printf(" name: %s\n", name);
    printf(" read: %s\n", rfile ? rfile : "none");
    printf(" verbose: %d\n", verbose);
    printf(" daemon: %s\n", done ? "no" : "yes");
    printf(" flags: %s\n\n", flags);

    {
        ydb_res res;
        ydb *datablock;
        // ignore SIGPIPE.
        signal(SIGPIPE, SIG_IGN);
        // add a signal handler to quit this program.
        signal(SIGINT, HANDLER_SIGINT);

        if (verbose)
            ydb_log_severity = verbose;

        datablock = ydb_open(name);
        res = ydb_connect(datablock, NULL, flags);
        if (res)
        {
            printf("\nydb error: %s\n", ydb_res_str[res]);
            exit(EXIT_FAILURE);
        }

        if (rfile)
        {
            ydb_res res;
            FILE *fp = fopen(rfile, "r");
            res = ydb_parse(datablock, fp);
            if (fp)
                fclose(fp);
            if (res)
            {
                printf("\nydb error: %s %s\n", ydb_res_str[res], rfile);
                exit(EXIT_FAILURE);
            }
            fprintf(stdout, "[datablock]\n");
            ydb_dump(datablock, stdout);
            fprintf(stdout, "\n");
        }
        do
        {
            res = ydb_serve(datablock, 5000);
            // char buf[512] = {0};
            // ydb_read(datablock,
            // 	"interface:\n"
            // 	" ge1:\n"
            // 	"  enabled: %s\n", buf
            // 	);
            if (res || done)
                printf("%s, res = %s\n", done ? "done" : "not-done", ydb_res_str[res]);
        } while (!res && !done);
        ydb_close(datablock);
    }
    return 0;
}
