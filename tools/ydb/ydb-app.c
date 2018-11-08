#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/select.h>

#include "ylist.h"
#include "ydb.h"

static int done;
void HANDLER_SIGINT(int param)
{
    done = 1;
}

void usage(char *argv_0)
{
    char *p;
    char *pname = ((p = strrchr(argv_0, '/')) ? ++p : argv_0);
    printf("Usage : %s --role=ROLE [OPTION...]\n", pname);
    printf("\n\
  -n, --name YDB_NAME              The name of created YDB (YAML DataBlock).\n\
  -r, --role (pub|sub|loc)         Set the role.\n\
                                   pub(lisher): as distribution server\n\
                                   sub(scriber): as distribution client\n\
                                   loc(al): no connection to others\n\
  -a, --addr YDB_ADDR              The YAML DataBlock communication address.\n\
                                   e.g. us:///tmp/ydb\n\
  -s, --summary                    Print all data at the termination.\n\
  -N, --no-change-data             Not print the change data\n\
  -f, --file FILE                  Read data from FILE to send publisher\n\
  -w, --writeable                  Write data to the remote YDB\n\
  -u, --unsubscribe                Disable subscription\n\
  -R, --reconnect                  Retry to reconnect upon the communication failure\n\
  -d, --daemon                     Runs in daemon mode\n\
  -v, --verbose (debug|inout|info) Verbose mode for debug\n\
    , --read  PATH/TO/DATA         Read data from YDB.\n\
    , --write PATH/TO/DATA=DATA    Write data from YDB.\n\
    , --delete PATH/TO/DATA=DATA   Delete data from YDB.\n\
  -h, --help                       Display this help and exit\n\n");
}

void get_stdin(ydb *datablock)
{
    int ret;
    fd_set read_set;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0; // if 0, sometimes it is not captured.
    FD_ZERO(&read_set);
    FD_SET(STDIN_FILENO, &read_set);
    ret = select(STDIN_FILENO + 1, &read_set, NULL, NULL, &tv);
    if (ret < 0)
        return;
    if (FD_ISSET(STDIN_FILENO, &read_set))
    {
        ydb_parse(datablock, stdin);
    }
}

typedef struct _ydbcmd
{
    enum
    {
        CMD_READ,
        CMD_WRITE,
        CMD_DELETE,
    } type;
    char *data;
} ydbcmd;

int main(int argc, char *argv[])
{
    ydb_res res = YDB_ERROR;
    ydb *datablock = NULL;

    int c;
    char *name = NULL;
    char *addr = NULL;
    char *role = 0;
    char flags[64] = {""};
    int summary = 0;
    int no_change_data = 0;
    int verbose = 0;
    int timeout = 0;
    int daemon = 0;
    ylist *filelist = NULL;
    ylist *cmdlist = NULL;

    filelist = ylist_create();
    if (!filelist)
    {
        fprintf(stderr, "ydb error: %s\n", "file list creation failed.");
        goto end;
    }
    cmdlist = ylist_create();
    if (!cmdlist)
    {
        fprintf(stderr, "ydb error: %s\n", "ydbcmd list creation failed.");
        goto end;
    }

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
            {"read", required_argument, 0, 0},
            {"write", required_argument, 0, 0},
            {"delete", required_argument, 0, 0},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}};

        c = getopt_long(argc, argv, "n:r:a:sNf:wuRdv:h",
                        long_options, &index);
        if (c == -1)
            break;

        switch (c)
        {
        case 0:
            /* If this option set a flag, do nothing else now. */
            if (long_options[index].flag != 0)
                break;
            ydbcmd *ycmd = malloc(sizeof(ydbcmd));
            if (ycmd)
            {
                ycmd->data = optarg;
                if (strcmp(long_options[index].name, "read") == 0)
                {
                    ycmd->type = CMD_READ;
                    ylist_push_back(cmdlist, ycmd);
                }
                else if (strcmp(long_options[index].name, "write") == 0)
                {
                    ycmd->type = CMD_WRITE;
                    ylist_push_back(cmdlist, ycmd);
                }
                else if (strcmp(long_options[index].name, "delete") == 0)
                {
                    ycmd->type = CMD_DELETE;
                    ylist_push_back(cmdlist, ycmd);
                }
                else
                    free(ycmd);
            }
            break;
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
            ylist_push_back(filelist, optarg);
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
            break;
        case 'u':
            strcat(flags, ":u");
            break;
        case 'R':
            strcat(flags, ":r");
            break;
        case 'd':
            daemon = 1;
            timeout = 5000; // 5sec
            break;
        case 'h':
            usage(argv[0]);
            res = YDB_OK;
            goto end;
        case '?':
        default:
            usage(argv[0]);
            goto end;
        }
    }

    if (!role)
    {
        usage(argv[0]);
        goto end;
    }

    if (verbose > 0)
    {
        printf(" configured options:\n");
        printf("  name: %s\n", name);
        printf("  role: %s\n", role);
        printf("  addr: %s\n", addr);
        printf("  summary: %s\n", summary ? "yes" : "no");
        printf("  no-change-data: %s\n", no_change_data ? "yes" : "no");
        if (!ylist_empty(filelist))
        {
            printf("  file:\n");
            ylist_iter *iter = ylist_first(filelist);
            for (; !ylist_done(filelist, iter); iter = ylist_next(filelist, iter))
                printf("   - %s\n", (char *)ylist_data(iter));
        }
        if (!ylist_empty(cmdlist))
        {
            printf("  ycmd:\n");
            ylist_iter *iter = ylist_first(cmdlist);
            for (; !ylist_done(cmdlist, iter); iter = ylist_next(cmdlist, iter))
            {
                ydbcmd *ycmd = ylist_data(iter);
                printf("   - {%s: %s}\n",
                       (ycmd->type == CMD_READ) ? "read" : (ycmd->type == CMD_WRITE) ? "write" : "delete",
                       (char *)ycmd->data);
            }
        }
        printf("  verbose: %d\n", verbose);
        printf("  flags: %s\n", flags);
        printf("  daemon: %s\n\n", done ? "no" : "yes");
    }

    {
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
            goto end;
        }

        if (!no_change_data)
        {
            res = ydb_connect(datablock, "file://stdout", "w:");
            if (res)
            {
                fprintf(stderr, "ydb error: %s\n", ydb_res_str[res]);
                goto end;
            }
        }

        if (strncmp(role, "loc", 3) != 0)
        {
            strcat(flags, role);
            res = ydb_connect(datablock, addr, flags);
            if (res)
            {
                fprintf(stderr, "ydb error: %s\n", ydb_res_str[res]);
                goto end;
            }
        }

        get_stdin(datablock);

        if (!ylist_empty(filelist))
        {
            char *fname = ylist_pop_front(filelist);
            while (fname)
            {
                FILE *fp = fopen(fname, "r");
                res = ydb_parse(datablock, fp);
                if (fp)
                    fclose(fp);
                if (res)
                {
                    printf("\nydb error: %s %s\n", ydb_res_str[res], fname);
                    goto end;
                }
                fname = ylist_pop_front(filelist);
            }
        }

        if (daemon)
        {
            do
            {
                res = ydb_serve(datablock, timeout);
                if (res)
                {
                    printf("\nydb error: %s\n", ydb_res_str[res]);
                    goto end;
                }
            } while (!done);
        }
        else
        {
            if (!ylist_empty(cmdlist))
            {
                ydbcmd *ycmd = ylist_pop_front(cmdlist);
                while (ycmd)
                {
                    char *data;
                    switch (ycmd->type)
                    {
                    case CMD_READ:
                        data = ydb_path_read(datablock, "%s", ycmd->data);
                        if (data)
                            fprintf(stdout, "%s", data);
                        else
                            res = YDB_E_NO_ENTRY;
                        break;
                    case CMD_WRITE:
                        res = ydb_path_write(datablock, "%s", ycmd->data);
                        break;
                    case CMD_DELETE:
                        res = ydb_path_delete(datablock, "%s", ycmd->data);
                        break;
                    }
                    if (res)
                    {
                        printf("\nydb error: %s (%s)\n", ydb_res_str[res], ycmd->data);
                        free(ycmd);
                        goto end;
                    }
                    free(ycmd);
                    ycmd = ylist_pop_front(cmdlist);
                }
            }
        }

        if (summary)
        {
            fprintf(stdout, "\n# %s\n", ydb_name(datablock));
            ydb_dump(datablock, stdout);
        }
    }
end:
    ydb_close(datablock);
    ylist_destroy(cmdlist);
    ylist_destroy(filelist);
    if (res)
        return 1;
    return 0;
}
