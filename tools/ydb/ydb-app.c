#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/select.h>

#include "ydb.h"

static int done = 1;
void HANDLER_SIGINT(int param)
{
    done = 1;
}

void usage(int status, char *progname)
{
    if (status != 0)
        fprintf(stderr, "Try `%s --help' for more information.\n", progname);
    else
    {
        printf("Usage : %s --role=ROLE [OPTION...]\n", progname);
        printf("\
ydb (YAML DATABLOCK)\n\
  -n, --name YDB_NAME       The name of created YDB (YAML DataBlock).\n\
  -r, --role (pub|sub|loc)  Set the role.\n\
                            pub(lisher): as distribution server\n\
                            sub(scriber): as distribution client\n\
                            loc(al): no connection to others\n\
  -a, --addr YDB_ADDR       The YAML DataBlock communication address.\n\
                            e.g. us:///tmp/ydb\n\
  -s, --summary             Print all data at the termination.\n\
  -N, --no-change-data      Not print the change data\n\
  -f, --file FILE           Read data from FILE to send publisher\n\
  -w, --writeable           Write data to the remote YDB\n\
  -u, --unsubscribe         Disable subscription\n\
  -R, --reconnect           Retry to reconnect upon the communication failure\n\
  -d, --daemon              Runs in daemon mode\n\
  -v, --verbose             Verbose mode for debug (debug|inout|info)\n\
  -h, --help                Display this help and exit\n\n");

    }
    exit(status);
}

void get_stdin(ydb *datablock)
{
    int ret;
    fd_set read_set;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1000; // if 0, sometimes it is not captured.
    FD_ZERO(&read_set);
    FD_SET(STDIN_FILENO, &read_set);
    ret = select(STDIN_FILENO + 1, &read_set, NULL, NULL, &tv);
    if (ret < 0) {
        return;
    }
    if (FD_ISSET(STDIN_FILENO, &read_set))
    {
        ydb_parse(datablock, stdin);
    }
}

int main(int argc, char *argv[])
{
    int c;
    char *p;
    char *progname = ((p = strrchr(argv[0], '/')) ? ++p : argv[0]);
    char *name = NULL;
    char *addr = NULL;
    char *rfile = NULL;
    char *role = 0;
    char flags[64] = {""};
    int summary = 0;
    int no_change_data = 0;
    int verbose = 0;
    int timeout = 0;

    while (1)
    {
        int index = 0;
        static struct option long_options[] = {
            {"name", required_argument, 0, 'n'},
            {"role", required_argument, 0, 'r'},
            {"addr", required_argument, 0, 'a'},
            {"summary", no_argument, 0, 's'},
            {"no-change-data", no_argument, 0, 'N'},
            {"file", required_argument, 0, 'f'},
            {"writeable", no_argument, 0, 'w'},
            {"unsubscribe", no_argument, 0, 'u'},
            {"reconnect", no_argument, 0, 'R'},
            {"daemon", no_argument, 0, 'd'},
            {"verbose", required_argument, 0, 'v'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}};

        c = getopt_long(argc, argv, "n:r:a:sNf:wuRdv:h",
                        long_options, &index);
        if (c == -1)
            break;

        switch (c)
        {
        case 'n':
            name = optarg;
            break;
        case 'r':
            role = optarg;
            break;
        case 'a':
            addr = optarg;
            break;
        case 's':
            summary = 1;
            break;
        case 'N':
            no_change_data = 1;
            break;
        case 'f':
            rfile = optarg;
            break;
        case 'v':
            if (strcmp(optarg, "debug") == 0)
                verbose = YDB_LOG_DEBUG;
            else if (strcmp(optarg, "inout") == 0)
                verbose = YDB_LOG_INOUT;
            else if (strcmp(optarg, "info") == 0)
                verbose = YDB_LOG_INFO;
            break;
        case 'w':
            strcat(flags, ":w");
        case 'u':
            strcat(flags, ":u");
            break;
        case 'R':
            strcat(flags, ":r");
            break;
        case 'd':
            done = 0;
            timeout = 5000; // 5sec
            break;
        case 'h':
            usage(0, progname);
        case '?':
        default:
            usage(1, progname);
        }
    }

    if (!role)
        usage(1, progname);
    
    if (verbose > 0)
    {
        printf(" configured options:\n");
        printf("  name: %s\n", name);
        printf("  role: %s\n", role);
        printf("  addr: %s\n", addr);
        printf("  summary: %s\n", summary?"yes":"no");
        printf("  no-change-data: %s\n", no_change_data?"yes":"no");
        printf("  file: %s\n", rfile ? rfile : "none");
        printf("  verbose: %d\n", verbose);
        printf("  flags: %s\n", flags);
        printf("  daemon: %s\n\n", done ? "no" : "yes");
    }

    {
        ydb_res res;
        ydb *datablock;
        // ignore SIGPIPE.
        signal(SIGPIPE, SIG_IGN);
        // add a signal handler to quit this program.
        signal(SIGINT, HANDLER_SIGINT);

        if (verbose)
            ydb_log_severity = verbose;

        datablock = ydb_open(name ? name : "top");
        if (!datablock)
        {
            fprintf(stderr, "ydb error: %s\n", "ydb failed");
            exit(EXIT_FAILURE);
        }

        if (!no_change_data)
        {
            res = ydb_connect(datablock, "file://stdout", "w:");
            if (res)
            {
                fprintf(stderr, "ydb error: %s\n", ydb_res_str[res]);
                ydb_close(datablock);
                exit(EXIT_FAILURE);
            }
        }
        
        if (strncmp(role, "loc", 3) != 0)
        {
            strcat(flags, role);
            res = ydb_connect(datablock, addr, flags);
            if (res)
            {
                fprintf(stderr, "ydb error: %s\n", ydb_res_str[res]);
                ydb_close(datablock);
                exit(EXIT_FAILURE);
            }
        }

        get_stdin(datablock);
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
                ydb_close(datablock);
                exit(EXIT_FAILURE);
            }
            // fprintf(stdout, "[datablock]\n");
            // ydb_dump(datablock, stdout);
            // fprintf(stdout, "\n");
        }

        do
        {
            res = ydb_serve(datablock, timeout);
            // if (res || done)
            //     printf("%s, res = %s\n", done ? "done" : "not-done", ydb_res_str[res]);
        } while (!res && !done);

        if (summary)
        {
            fprintf(stdout, "\n# %s\n", ydb_name(datablock));
            ydb_dump(datablock, stdout);
        }
        ydb_close(datablock);
    }
    return 0;
}
