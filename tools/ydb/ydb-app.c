#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/select.h>

#include "config.h"
#include "ylog.h"
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
    printf("\n");
    printf(" version: %s\n", VERSION);
    printf(" bug report: %s\n", PACKAGE_BUGREPORT);
    printf("\n");
    printf(" usage : %s [OPTION...]\n", pname);
    printf("\n\
  -n, --name YDB_NAME              The name of created YDB (YAML DataBlock).\n\
  -r, --role (pub|sub|loc)         Set the role.\n\
                                   pub(publisher): as distribution server\n\
                                   sub(subscriber): as distribution client\n\
                                   loc(local): no connection to others\n\
  -a, --addr YDB_ADDR              The YAML DataBlock communication address.\n\
                                   e.g. us:///tmp/ydb\n\
  -s, --summary                    Print all data at the termination.\n\
  -N, --no-change-data             Not print the change data\n\
  -f, --file FILE                  Read data from FILE to send publisher\n\
  -w, --writeable                  Write data to the remote YDB\n\
  -u, --unsubscribe                Disable subscription\n\
  -S, --sync-before-read           update YDB from remotes before read\n\
  -d, --daemon                     Runs in daemon mode\n\
  -i, --interpret                  Interpret mode\n\
  -v, --verbose (debug|inout|info) Verbose mode for debug\n\
    , --read PATH/TO/DATA          Read data (value only) from YDB.\n\
    , --print PATH/TO/DATA         Print data from YDB.\n\
    , --write PATH/TO/DATA=DATA    Write data to YDB.\n\
    , --delete PATH/TO/DATA=DATA   Delete data from YDB.\n\
    , --sync PATH/TO/DATA=DATA     Update data using sync message.\n\
  -h, --help                       Display this help and exit\n\n");
}

void get_yaml_from_stdin(ydb *datablock)
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

void interpret_mode_help()
{
    fprintf(stdout, "\n\
 [YDB Interpret mode commands]\n\n\
  write  /path/to/data=DATA   Write DATA to /path\n\
  delete /path/to/data        Delete DATA from /path\n\
  read   /path/to/data        Read DATA (value only) from /path\n\
  print  /path/to/data        Print DATA (all) in /path\n\
  dump   (FILE | )            Dump all DATA to FILE\n\
  parse  FILE                 Parse DATA from FILE\n\
  quit                        quit\n\n");
}

ydb_res interpret_mode_run(ydb *datablock)
{
    ydb_res res = YDB_OK;
    int n = 0;
    char op = 0;
    char buf[512];
    char *value;
    char *path;
    char *cmd;

    cmd = fgets(buf, sizeof(buf), stdin);
    if (!cmd)
    {
        fprintf(stderr, "%% no command to run\n");
        interpret_mode_help();
        return YDB_ERROR;
    }
    cmd = strtok(buf, " \n\t");
    if (!cmd)
    {
        fprintf(stdout, ">\n");
        fprintf(stderr, "%% no command to run\n");
        interpret_mode_help();
        return YDB_ERROR;
    }
    if (strncmp(cmd, "write", 1) == 0)
        op = 'w';
    else if (strncmp(cmd, "delete", 2) == 0)
        op = 'd';
    else if (strncmp(cmd, "read", 1) == 0)
        op = 'r';
    else if (strncmp(cmd, "print", 2) == 0)
        op = 'p';
    else if (strncmp(cmd, "dump", 2) == 0)
        op = 'D';
    else if (strncmp(cmd, "parse", 2) == 0)
        op = 'P';
    else if (strncmp(cmd, "quit", 1) == 0)
    {
        fprintf(stdout, "%% quit\n");
        done = 1;
        return YDB_OK;
    }
    if (op == 0)
    {
        fprintf(stderr, "%% unknown command\n");
        interpret_mode_help();
        return YDB_ERROR;
    }
    path = strtok(NULL, " \n\t");

    fprintf(stdout, "> %s %s\n", cmd, path ? path : "");

    switch (op)
    {
    case 'w':
        res = ydb_path_write(datablock, "%s", path);
        break;
    case 'd':
        res = ydb_path_delete(datablock, "%s", path);
        break;
    case 'r':
        value = ydb_path_read(datablock, "%s", path);
        if (!value)
        {
            res = YDB_ERROR;
            break;
        }
        fprintf(stdout, "%s\n", value);
        fflush(stdout);
        break;
    case 'p':
        n = ydb_path_fprintf(stdout, datablock, "%s", path);
        if (n < 0)
            res = YDB_ERROR;
        break;
    case 'D':
        if (path)
        {
            FILE *dumpfp = fopen(path, "w");
            if (dumpfp)
            {
                n = ydb_dump(datablock, dumpfp);
                fclose(dumpfp);
            }
        }
        else
        {
            n = ydb_dump(datablock, stdout);
            fflush(stdout);
        }
        if (n < 0)
            res = YDB_ERROR;
        break;
    case 'P':
        if (path)
        {
            FILE *parsefp = fopen(path, "r");
            if (parsefp)
            {
                n = ydb_parse(datablock, parsefp);
                fclose(parsefp);
            }
        }
        else
            n = ydb_parse(datablock, stdout);
        if (n < 0)
            res = YDB_ERROR;
        break;
    default:
        break;
    }
    if (YDB_FAILED(res))
        fprintf(stderr, "%% command (%s) failed (%s)\n", cmd, ydb_res_str(res));
    return res;
}

typedef struct _ydbcmd
{
    enum
    {
        CMD_READ,
        CMD_PRINT,
        CMD_WRITE,
        CMD_DELETE,
        CMD_SYNC,
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
    int change_log = 0;
    int verbose = 0;
    int timeout = 0;
    int daemon = 0;
    int interpret = 0;
    ylist *filelist = NULL;
    ylist *cmdlist = NULL;

    if (argc <= 1)
    {
        usage(argv[0]);
        return 0;
    }

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
            {"change-log", no_argument, 0, 'c'},
            {"file", required_argument, 0, 'f'},
            {"writeable", no_argument, 0, 'w'},
            {"unsubscribe", no_argument, 0, 'u'},
            {"sync-before-read", no_argument, 0, 'S'},
            {"daemon", no_argument, 0, 'd'},
            {"interpret", no_argument, 0, 'i'},
            {"verbose", required_argument, 0, 'v'},
            {"read", required_argument, 0, 0},
            {"print", required_argument, 0, 0},
            {"write", required_argument, 0, 0},
            {"delete", required_argument, 0, 0},
            {"sync", required_argument, 0, 0},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}};

        c = getopt_long(argc, argv, "n:r:a:scf:wuSRdiv:h",
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
                else if (strcmp(long_options[index].name, "print") == 0)
                {
                    ycmd->type = CMD_PRINT;
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
                else if (strcmp(long_options[index].name, "sync") == 0)
                {
                    ycmd->type = CMD_SYNC;
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
            if (strncmp(optarg, "pub", 3) == 0)
                role = "pub";
            else if (strncmp(optarg, "sub", 3) == 0)
                role = "sub";
            else if (strncmp(optarg, "loc", 3) == 0)
                role = "local";
            else
            {
                fprintf(stderr, "\n invalid role configured (%s)\n", optarg);
                usage(argv[0]);
            }
            break;
        case 'a':
            addr = optarg;
            break;
        case 's':
            summary = 1;
            break;
        case 'c':
            change_log = 1;
            break;
        case 'f':
            ylist_push_back(filelist, optarg);
            break;
        case 'v':
            if (strcmp(optarg, "debug") == 0)
                verbose = YLOG_DEBUG;
            else if (strcmp(optarg, "inout") == 0)
                verbose = YLOG_INOUT;
            else if (strcmp(optarg, "info") == 0)
                verbose = YLOG_INFO;
            break;
        case 'w':
            strcat(flags, ":writeable");
            break;
        case 'u':
            strcat(flags, ":unsubscribe");
            break;
        case 'S':
            strcat(flags, ":sync-before-read");
            break;
        case 'd':
            daemon = 1;
            timeout = 5000; // 5sec
            break;
        case 'i':
            interpret = 1;
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
        role = "local";
    }

    if (verbose > 0)
    {
        printf(" configured options:\n");
        printf("  name: %s\n", name);
        printf("  role: %s\n", role);
        printf("  addr: %s\n", addr);
        printf("  summary: %s\n", summary ? "yes" : "no");
        printf("  change-log: %s\n", change_log ? "yes" : "no");
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
                       (ycmd->type == CMD_READ) ? "read" :
                       (ycmd->type == CMD_WRITE) ? "write" :
                       (ycmd->type == CMD_SYNC) ? "sync" : "delete",
                       (char *)ycmd->data);
            }
        }
        printf("  verbose: %d\n", verbose);
        printf("  flags: %s\n", flags);
        printf("  daemon: %s\n\n", done ? "no" : "yes");
        printf("  interpret: %s\n\n", interpret ? "yes" : "no");
    }

    {
        // ignore SIGPIPE.
        signal(SIGPIPE, SIG_IGN);
        // add a signal handler to quit this program.
        signal(SIGINT, HANDLER_SIGINT);

        if (verbose)
            ylog_severity = verbose;

        datablock = ydb_open(name ? name : "top");
        if (!datablock)
        {
            fprintf(stderr, "ydb error: %s\n", "ydb failed");
            goto end;
        }

        if (change_log)
        {
            res = ydb_connect(datablock, "file://stdout", "w:");
            if (res)
            {
                fprintf(stderr, "ydb error: %s\n", ydb_res_str(res));
                goto end;
            }
        }

        if (strncmp(role, "loc", 3) != 0)
        {
            strcat(flags, role);
            res = ydb_connect(datablock, addr, flags);
            if (res)
            {
                fprintf(stderr, "ydb error: %s\n", ydb_res_str(res));
                goto end;
            }
        }

        get_yaml_from_stdin(datablock);

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
                    printf("\nydb error: %s %s\n", ydb_res_str(res), fname);
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
                if (YDB_FAILED(res))
                {
                    fprintf(stderr, "\nydb error: %s\n", ydb_res_str(res));
                    goto end;
                }
                else if (YDB_WARNING(res) && (verbose == YLOG_DEBUG))
                    printf("\nydb warning: %s\n", ydb_res_str(res));
            } while (!done);
        }
        else if (interpret)
        {
            do
            {
                int ret, fd;
                fd_set read_set;
                struct timeval tv;
                tv.tv_sec = timeout / 1000;
                tv.tv_usec = (timeout % 1000) * 1000;
                FD_ZERO(&read_set);
                fd = ydb_fd(datablock);
                if (fd < 0)
                    break;

                FD_SET(fd, &read_set);
                FD_SET(STDIN_FILENO, &read_set);
                if (interpret == 1)
                {
                    interpret++;
                    interpret_mode_help();
                }
                ret = select(fd + 1, &read_set, NULL, NULL, &tv);
                if (ret < 0)
                {
                    fprintf(stderr, "\nselect error: %s\n", strerror(errno));
                    done = 1;
                }
                else if (ret == 0 || FD_ISSET(fd, &read_set))
                {
                    FD_CLR(fd, &read_set);
                    res = ydb_serve(datablock, 0);
                    if (YDB_FAILED(res))
                    {
                        fprintf(stderr, "\nydb error: %s\n", ydb_res_str(res));
                        done = 1;
                    }
                    else if (YDB_WARNING(res))
                        printf("\nydb warning: %s\n", ydb_res_str(res));
                }
                else
                {
                    if (FD_ISSET(STDIN_FILENO, &read_set))
                    {
                        FD_CLR(STDIN_FILENO, &read_set);
                        interpret_mode_run(datablock);
                    }
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
                    switch (ycmd->type)
                    {
                    case CMD_READ:
                    {
                        char *data = ydb_path_read(datablock, "%s", ycmd->data);
                        if (data)
                            fprintf(stdout, "%s", data);
                        else
                            res = YDB_E_NO_ENTRY;
                        break;
                    }
                    case CMD_PRINT:
                    {
                        ydb_path_fprintf(stdout, datablock, "%s", ycmd->data);
                        break;
                    }
                    case CMD_WRITE:
                        res = ydb_path_write(datablock, "%s", ycmd->data);
                        break;
                    case CMD_DELETE:
                        res = ydb_path_delete(datablock, "%s", ycmd->data);
                        break;
                    case CMD_SYNC:
                        res = ydb_path_sync(datablock, "%s", ycmd->data);
                        break;
                    default:
                        break;
                    }
                    if (YDB_FAILED(res))
                        printf("\nydb error: %s (%s)\n", ydb_res_str(res), ycmd->data);
                    // else if (YDB_WARNING(res))
                    //     printf("\nydb warning: %s (%s)\n", ydb_res_str(res), ycmd->data);

                    free(ycmd);
                    ycmd = ylist_pop_front(cmdlist);
                    if (ycmd)
                        fprintf(stdout, " ");
                }
            }
        }

        if (summary)
        {
            fprintf(stdout, "\n# %s\n", ydb_name(datablock));
            if (verbose == YLOG_DEBUG)
                ydb_dump_debug(datablock, stdout);
            else
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
