#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/select.h>

#ifdef ENABLED_PLOG
#include "plog.h"
#endif

#include "config.h"
#include "ylog.h"
#include "ylist.h"
#include "ydb.h"
#include "ytimer.h"

extern int tx_fail_count;
extern int tx_fail_en;

static int done;
void SIG_QUIT(int param)
{
    done = 1;
}

#ifdef ENABLED_PLOG
void plog_init()
{
    if (access("/mnt/flash/log", X_OK) == 0)
    {
        plog_file_mgmt(1);
        plog_dir("/mnt/flash/log");
    }
    plog_traces(NULL, 0);
    // plog_logging_enable(SIGCHLD, 0);
    plog_logging_enable(SIGINT, 0);
    plog_logging_enable(SIGPIPE, 0);
    plog_logging_enable(SIGTERM, 0);
    plog_signal(SIGINT, SIG_QUIT);
    plog_signal(SIGPIPE, SIG_IGN);
}

void plog_cleanup()
{
    plog_untraces(NULL, 0);
}
#endif
void usage(char *argv_0)
{
    char *p;
    char *pname = ((p = strrchr(argv_0, '/')) ? ++p : argv_0);
    printf("\n");
    printf(" YDB (YAML Datablock)\n");
    printf(" version: %s\n", VERSION);
    printf(" bug report: %s\n", PACKAGE_BUGREPORT);
    printf("\n");
    printf(" usage : %s [OPTION...]\n", pname);
    printf("\n\
  -n, --name NAME                  The name of created YDB (YAML DataBlock).\n\
  -w, --writable                   Send updated data to YDB publisher.\n\
  -u, --unsubscribe                Disable subscription.\n\
  -S, --sync-before-read           update data from YDB publishers.\n\
                                   -w, -u and -S options should be ahead of -a YDB_ADDR\n\
  -r, --role (pub|sub|loc)         Set the role.\n\
                                   pub(publisher): as distribution server\n\
                                   sub(subscriber): as distribution client\n\
                                   loc(local): no connection (default)\n\
  -a, --addr YDB_ADDR              The YAML DataBlock communication address\n\
                                   e.g. us:///SOCKET_FILE (unix socket)\n\
                                        uss://SOCKET_FILE\n\
                                        (unix socket hidden from file system)\n\
                                        tcp://IPADDR:PORT (TCP)\n\
                                        file://FILEPATH (file)\n\
  -s, --summary                    Print all data at the termination.\n\
  -c, --change-log                 print all change.\n\
  -f, --file FILE                  Read YAML file to update YDB.\n\
  -d, --daemon                     Runs on daemon mode.\n\
  -i, --interpret                  Runs on interpret mode.\n\
  -I, --input                      Standard Input\n\
  -t, --timeout                    Set timeout of sync\n\
  -v, --verbose (debug|inout|info) Verbose mode for debug\n\
  -l, --logging-config             Enable Runtime logging configuration\n\
            ydb -r pub -a uss://test --write '+logging/ydbhub/level=debug'\\\n\
                       --write '+logging/ydbhub/logfile=/tmp/ydb.hub1.log'\n\
    , --record-to FILE             Record the published data to FILE\n\
    , --replay-from FILE           Replay the recorded FILE\n\
    , --replay-interval MSEC       Set replay time of each YAML data in FILE\n\
    , --read PATH/TO/DATA          Read data (value only) from YDB.\n\
    , --print PATH/TO/DATA         Print data from YDB.\n\
    , --write PATH/TO/DATA=DATA    Write data to YDB.\n\
    , --delete PATH/TO/DATA=DATA   Delete data from YDB.\n\
    , --sync PATH/TO/DATA=DATA     Send sync request to update data.\n\
  -h, --help                       Display help and exit\n\n\
  e.g.\n\
    ydb -n mydata -r pub -a uss://mydata -d -f example/yaml/yaml-demo.yaml &\n\
    ydb -n mydata -r sub -a uss://mydata -i\n\n\
  ");
}

void get_yaml_from_stdin(ydb *datablock)
{
    int ret;
    fd_set read_set;
    struct timeval tv;
    tv.tv_sec = 3;
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
  write   /path/to/data=DATA   Write DATA to /path\n\
  delete  /path/to/data        Delete DATA from /path\n\
  read    /path/to/data        Read DATA (value only) from /path\n\
  print   /path/to/data        Print DATA (all) in /path\n\
  sync    /path/to/data        Request sync of /path\n\
  conn    address flags        Connect to communication channel\n\
  disconn address              Disconnect from communication channel\n\
  dump    (FILE | )            Dump all DATA to FILE\n\
  parse   FILE                 Parse DATA from FILE\n\
  quit                         quit\n\n");
}

ydb_res interpret_mode_run(ydb *datablock)
{
    ydb_res res = YDB_OK;
    int n = 0;
    char op = 0;
    char buf[512];
    const char *value;
    char *path;
    char *cmd;
    char *flags;

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
    else if (strncmp(cmd, "sync", 2) == 0)
        op = 's';
    else if (strncmp(cmd, "delete", 2) == 0)
        op = 'd';
    else if (strncmp(cmd, "read", 1) == 0)
        op = 'r';
    else if (strncmp(cmd, "print", 2) == 0)
        op = 'p';
    else if (strncmp(cmd, "connect", 2) == 0)
        op = 'c';
    else if (strncmp(cmd, "disconnect", 2) == 0)
        op = 'C';
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
    case 'c':
        flags = strtok(NULL, " \n\t");
        if (!flags)
            flags = "pub";
        res = ydb_connect(datablock, path, flags);
        break;
    case 'C':
        res = ydb_disconnect(datablock, path);
        break;
    case 'w':
        res = ydb_path_write(datablock, "%s", path);
        break;
    case 'd':
        res = ydb_path_delete(datablock, "%s", path);
        break;
    case 's':
        res = ydb_path_sync(datablock, "%s", path);
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

ytimer_status replay(ytimer *timer, unsigned int timer_id, ytimer_status status, void *user1, void *user2)
{
    static int msgqueue[6];
    static FILE *msgfp;
    static char *buf;
    static size_t buflen;
    ydb *datablock = user1;
    FILE *recordfp = user2;
    bool start_replay = false;

    if (status == YTIMER_ABORTED || recordfp == NULL || datablock == NULL)
        goto _replay_end;
    if (msgfp == NULL)
    {
        msgfp = open_memstream(&buf, &buflen);
        if (msgfp == NULL)
        {
            status = YTIMER_ABORTED;
            goto _replay_end;
        }
    }
    while (!feof(recordfp))
    {
        int c = fgetc(recordfp);
        msgqueue[0] = msgqueue[1];
        msgqueue[1] = msgqueue[2];
        msgqueue[2] = msgqueue[3];
        msgqueue[3] = msgqueue[4];
        msgqueue[4] = c;
        fwrite(&c, 1, 1, msgfp);
        if (msgqueue[0] == '\n')
        {
            if (msgqueue[1] == '-' && msgqueue[2] == '-' && msgqueue[3] == '-' && msgqueue[4] == '\n')
            {
                fflush(msgfp);
                if (buflen > sizeof("\n---\n") - 1)
                { // strlen("\n---\n")
                    start_replay = true;
                    break;
                }
            }
            else if (msgqueue[1] == '.' && msgqueue[2] == '.' && msgqueue[3] == '.' && msgqueue[4] == '\n')
            {
                start_replay = true;
                break;
            }
        }
    }

    if (start_replay)
    {
        fclose(msgfp);
        if (buf)
        {
            ydb_parses(datablock, buf, buflen);
            free(buf);
        }
        msgfp = NULL;
        buf = NULL;
        buflen = 0;
    }
    if (feof(recordfp))
    {
        // stop the periodic replay func
        status = YTIMER_COMPLETED;
        goto _replay_end;
    }
    return YTIMER_NO_ERR;
_replay_end:
    if (msgfp)
        fclose(msgfp);
    if (buf)
        free(buf);
    msgfp = NULL;
    buf = NULL;
    buflen = 0;
    return status;
}

void logging_config(ydb *datablock, char op, ynode *base, ynode *cur, ynode *_new)
{
    if (op == 'd')
    {
        ylog_severity = YLOG_ERROR;
        ylog_file_close();
    }
    else
    {
        ynode *level = ydb_find_child(base, "level");
        ynode *logfile = ydb_find_child(base, "logfile");
        if (logfile && ydb_value(logfile))
            ylog_file_open("%s", ydb_value(logfile));
        if (level && ydb_value(level))
        {
            if (strcmp(ydb_value(level), "none") == 0)
                ylog_severity = YLOG_CRITICAL;
            else if (strcmp(ydb_value(level), "error") == 0)
                ylog_severity = YLOG_ERROR;
            else if (strcmp(ydb_value(level), "warn") == 0)
                ylog_severity = YLOG_WARN;
            else if (strcmp(ydb_value(level), "info") == 0)
                ylog_severity = YLOG_INFO;
            else if (strcmp(ydb_value(level), "inout") == 0)
                ylog_severity = YLOG_INOUT;
            else if (strcmp(ydb_value(level), "debug") == 0)
                ylog_severity = YLOG_DEBUG;
        }
    }
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
    char *addr = NULL;
    char *role = NULL;
    char con_flags[128] = {
        0,
    };
    int verbose = 0;
    int timeout = YDB_DEFAULT_TIMEOUT;
    int daemon_timeout = 0;
    int daemon = 0;
    int interpret = 0;
    int no_rx = 0;
    int replay_msec = 0;
    char replay_from[256 + 7] = "file:///tmp/ydb.record.yaml";
    ydb *config = NULL;

    if (argc <= 1)
    {
        usage(argv[0]);
        return 0;
    }

    config = ydb_open("config");
    if (!config)
    {
        fprintf(stderr, "ydb error: %s\n", "ydb[config] open failed.");
        goto end;
    }
    ydb_write(config,
              "config:\n"
              " name: ydb.%d\n"
              " summary: false\n"
              " change-log: false\n"
              " verbose: none\n"
              " timeout: 0 ms\n"
              " daemon: false\n"
              " interpret: false\n",
              getpid());

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
            {"input", no_argument, 0, 'I'},
            {"timeout", required_argument, 0, 't'},
            {"verbose", required_argument, 0, 'v'},
            {"logging-config", no_argument, 0, 'l'},
            // record and replay
            {"record-to", required_argument, 0, 0},
            {"replay-from", required_argument, 0, 0},
            {"replay-interval", required_argument, 0, 0},
            // commands
            {"read", required_argument, 0, 0},
            {"print", required_argument, 0, 0},
            {"write", required_argument, 0, 0},
            {"delete", required_argument, 0, 0},
            {"sync", required_argument, 0, 0},
            // diagnositics
            {"no-rx", no_argument, 0, 0},
            {"no-tx", no_argument, 0, 0},
            {"tx-fail", required_argument, 0, 0},
            //
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}};

        c = getopt_long(argc, argv, "n:r:a:scf:wuSRdiIt:v:lh",
                        long_options, &index);
        if (c == -1)
            break;

        switch (c)
        {
        case 0:
            /* If this option set a flag, do nothing else now. */
            if (long_options[index].flag != 0)
                break;
            if (strcmp(long_options[index].name, "read") == 0 ||
                strcmp(long_options[index].name, "print") == 0 ||
                strcmp(long_options[index].name, "write") == 0 ||
                strcmp(long_options[index].name, "delete") == 0 ||
                strcmp(long_options[index].name, "sync") == 0)
            {
                ydb_write(config,
                          "config:\n"
                          " command:\n"
                          "  - {type: %s, path: '%s'}\n",
                          long_options[index].name,
                          optarg);
            }
            else if (strcmp(long_options[index].name, "record-to") == 0)
            {
                char *yamloptstr = str2yaml(optarg);
                ydb_write(config,
                          "config:\n"
                          " record:\n"
                          "  %s: %s\n",
                          long_options[index].name,
                          yamloptstr);
                free(yamloptstr);
            }
            else if (
                strcmp(long_options[index].name, "replay-from") == 0 ||
                strcmp(long_options[index].name, "replay-interval") == 0)
            {
                char *yamloptstr = str2yaml(optarg);
                ydb_write(config,
                          "config:\n"
                          " replay:\n"
                          "  %s: %s\n",
                          long_options[index].name,
                          yamloptstr);
                free(yamloptstr);
            }
            else
            {
                if (strcmp(long_options[index].name, "tx-fail") == 0)
                {
                    tx_fail_en = 1;
                    tx_fail_count = atoi(optarg);
                }
                else if (strcmp(long_options[index].name, "no-tx") == 0)
                {
                    tx_fail_en = 1;
                    tx_fail_count = -1;
                }
                else if (strcmp(long_options[index].name, "no-rx") == 0)
                {
                    no_rx = 1;
                }
                if (optarg)
                    ydb_write(config,
                              "config:\n"
                              " test:\n"
                              "  %s: %s\n",
                              long_options[index].name, optarg);
                else
                    ydb_write(config,
                              "config:\n"
                              " test:\n"
                              "  %s: true\n",
                              long_options[index].name);
            }
            break;
        case 'n':
            ydb_write(config, "config: {name: '%s'}", optarg);
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
                goto end;
            }
            break;
        case 'a':
            addr = optarg;
            if (role == NULL)
            {
                fprintf(stderr, "\n no role configured for %s\n", optarg);
                goto end;
            }
            if ((role && (strncmp(role, "loc", 3) == 0)))
            {
                fprintf(stderr, "\n invalid role configured (%s)\n", role);
                goto end;
            }
            ydb_write(config,
                      "config: {connection: [{ addr: '%s', flags: '%s%s', timeout: %d}]}\n",
                      addr, role, con_flags[0] ? con_flags : "", timeout);
            memset(con_flags, 0x0, sizeof(con_flags));
            addr = NULL;
            break;
        case 's':
            ydb_write(config, "config: {summary: true}");
            break;
        case 'c':
            ydb_write(config, "config: {change-log: true}");
            break;
        case 'f':
            ydb_write(config, "config: {file: [%s]}", optarg);
            break;
        case 'v':
            if (strcmp(optarg, "debug") == 0)
                verbose = YLOG_DEBUG;
            else if (strcmp(optarg, "inout") == 0)
                verbose = YLOG_INOUT;
            else if (strcmp(optarg, "info") == 0)
                verbose = YLOG_INFO;
            else
            {
                fprintf(stderr, "\n invalid verbose configured (%s)\n", optarg);
                usage(argv[0]);
                goto end;
            }
            ydb_write(config, "config: {verbose: %s}", optarg);
            break;
        case 'l':
            ydb_write(config, "config: {logging-config: true}");
            break;
        case 'w':
            strcat(con_flags, ":writeable");
            break;
        case 'u':
            strcat(con_flags, ":unsubscribe");
            break;
        case 'S':
            strcat(con_flags, ":sync-before-read");
            break;
        case 'd':
            daemon = 1;
            daemon_timeout = 5000;
            ydb_write(config, "config: {daemon: true, timeout: %d ms}", daemon_timeout);
            break;
        case 't':
            timeout = atoi(optarg);
            break;
        case 'I':
            ydb_write(config, "config: {input: true}");
            break;
        case 'i':
            interpret = 1;
            ydb_write(config, "config: {interpret: true}");
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
    if (verbose > 0)
    {
        printf("[configured options]\n");
        ydb_dump(config, stdout);
        printf("\n");
    }

    {
#ifdef ENABLED_PLOG
        plog_init();
#else
        // ignore SIGPIPE.
        signal(SIGPIPE, SIG_IGN);
        // add a signal handler to quit this program.
        signal(SIGINT, SIG_QUIT);
#endif
        if (verbose)
            ylog_level = verbose;

        datablock = ydb_open((char *)ydb_path_read(config, "/config/name"));
        if (!datablock)
        {
            fprintf(stderr, "ydb error: %s\n", "ydb failed");
            goto end;
        }

        const char *lc = ydb_path_read(config, "/config/logging-config");
        if (lc && strcmp(lc, "true") == 0)
        {
            char p[256];
            snprintf(p, sizeof(p), "+logging/%s", ydb_path_read(config, "/config/name"));
            ydb_write_hook_add(datablock, p, 1, (ydb_write_hook)logging_config, 0);
        }

        if (strcmp(ydb_path_read(config, "/config/change-log"), "true") == 0)
        {
            ydb_connection_log(1);
            res = ydb_connect(datablock, "file://stdout", "pub");
            if (res)
            {
                fprintf(stderr, "ydb error: %s\n", ydb_res_str(res));
                goto end;
            }
        }

        ynode *node = ydb_search(config, "/config/record");
        if (!ydb_empty(node))
        {
            char record_to[256 + 7] = "file:///tmp/ydb.record.yaml";
            ydb_read(config,
                     "config:\n"
                     " record:\n"
                     "  record-to: %s\n",
                     &record_to[7]);
            res = ydb_connect(datablock, record_to, "pub");
            if (res)
            {
                fprintf(stderr, "ydb error: %s\n", ydb_res_str(res));
                goto end;
            }
        }
        node = ydb_search(config, "/config/replay");
        if (!ydb_empty(node))
        {
            ydb_read(config,
                     "config:\n"
                     " replay:\n"
                     "  replay-interval: %d\n"
                     "  replay-from: %s\n",
                     &replay_msec,
                     &replay_from[7]);
            if (replay_msec <= 0)
            {
                fprintf(stderr, "invalid replay-interval\n");
                goto end;
            }
        }

        if (!ydb_empty(ydb_search(config, "/config/connection")))
        {
            ynode *n;
            // ydb_connection_log(1);
            n = ydb_search(config, "/config/connection");
            for (n = ydb_down(n); n; n = ydb_next(n))
            {
                const char *a = ydb_value(ydb_find(n, "addr"));
                const char *f = ydb_value(ydb_find(n, "flags"));
                const char *t = ydb_value(ydb_find(n, "timeout"));
                int to = atoi(t);
                if (to != YDB_DEFAULT_TIMEOUT)
                    ydb_timeout(datablock, to);
                res = ydb_connect(datablock, (char *)a, (char *)f);
                if (res)
                {
                    fprintf(stderr, "ydb error: %s\n", ydb_res_str(res));
                    goto end;
                }
            }
        }

        if (!ydb_empty(ydb_search(config, "/config/input")))
            get_yaml_from_stdin(datablock);

        if (!ydb_empty(ydb_search(config, "/config/file")))
        {
            ynode *n;
            n = ydb_search(config, "/config/file");
            n = ydb_down(n);
            while (n)
            {
                FILE *fp = fopen(ydb_value(n), "r");
                if (fp)
                {
                    res = ydb_parse(datablock, fp);
                    fclose(fp);
                    if (res)
                    {
                        printf("\nydb error: %s %s\n", ydb_res_str(res), ydb_value(n));
                        goto end;
                    }
                }
                else
                {
                    printf("\nfile error: %s %s\n", ydb_value(n), strerror(errno));
                    goto end;
                }
                n = ydb_next(n);
            }
        }

        if (!ydb_empty(ydb_search(config, "/config/command")))
        {
            if (daemon || interpret)
            {
                fprintf(stderr, "\nydb error: commands are unable to run with -d or -i mode.\n");
                goto end;
            }
        }

        if (daemon && replay_msec <= 0)
        {
            while (!done)
            {
                if (no_rx)
                {
                    ylog_error("no-rx for test\n");
                    sleep(1);
                    continue;
                }
                res = ydb_serve(datablock, daemon_timeout);
                if (YDB_FAILED(res))
                {
                    fprintf(stderr, "\nydb error: %s\n", ydb_res_str(res));
                    goto end;
                }
                else if (YDB_WARNING(res) && (verbose == YLOG_DEBUG))
                    printf("\nydb warning: %s\n", ydb_res_str(res));
            }
        }
        else if (interpret || daemon || replay_msec > 0)
        {
            ytimer *timer = NULL;
            unsigned int timerid = 0;
            FILE *replay_fp = NULL;
            if (no_rx)
            {
                fprintf(stderr, "--no-rx running on daemon mode.\n");
                goto end;
            }
            if (replay_msec > 0)
            {
                replay_fp = fopen(&replay_from[7], "r");
                if (replay_fp == NULL)
                {
                    fprintf(stderr, "'%s' exists.\n", replay_from);
                    goto end;
                }
                timer = ytimer_create();
                timerid = ytimer_set_msec(timer, replay_msec, 1, (ytimer_func)replay, 2, datablock, replay_fp);
            }
            while (!done)
            {
                int ret, fd, timerfd = -1, fdmax;
                fd_set read_set;
                struct timeval tv;
                tv.tv_sec = daemon_timeout / 1000;
                tv.tv_usec = (daemon_timeout % 1000) * 1000;
                FD_ZERO(&read_set);
                fd = ydb_fd(datablock);
                if (fd < 0)
                    break;
                fdmax = fd;
                FD_SET(fd, &read_set);
                if (interpret == 1)
                    interpret_mode_help();
                if (interpret >= 1)
                {
                    interpret++;
                    FD_SET(STDIN_FILENO, &read_set);
                }
                if (timerid > 0)
                {
                    timerfd = ytimer_fd(timer);
                    FD_SET(timerfd, &read_set);
                    if (timerfd > fd)
                        fdmax = timerfd;
                }
                ret = select(fdmax + 1, &read_set, NULL, NULL, (daemon_timeout == 0) ? NULL : &tv);
                if (ret < 0)
                {
                    fprintf(stderr, "\nselect error: %s\n", strerror(errno));
                    done = 1;
                }
                else if (FD_ISSET(fd, &read_set))
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
                else if (timerfd >= 0 && FD_ISSET(timerfd, &read_set))
                {
                    FD_CLR(timerfd, &read_set);
                    ytimer_serve(timer);
                }
                else if (FD_ISSET(STDIN_FILENO, &read_set))
                {
                    FD_CLR(STDIN_FILENO, &read_set);
                    interpret_mode_run(datablock);
                }
            }
            if (timer)
                ytimer_destroy(timer);
            if (replay_fp)
                fclose(replay_fp);
        }
        else
        {
            if (!ydb_empty(ydb_search(config, "/config/command")))
            {
                ynode *n;
                n = ydb_search(config, "/config/command");
                for (n = ydb_down(n); n;)
                {
                    const char *type = ydb_value(ydb_find(n, "type"));
                    const char *path = ydb_value(ydb_find(n, "path"));
                    switch (type[0])
                    {
                    case 'r':
                    {
                        const char *data = ydb_path_read(datablock, "%s", path);
                        if (data)
                            fprintf(stdout, "%s", data);
                        break;
                    }
                    case 'p':
                        ydb_path_fprintf(stdout, datablock, "%s", path);
                        break;
                    case 'w':
                        res = ydb_path_write(datablock, "%s", path);
                        break;
                    case 'd':
                        res = ydb_path_delete(datablock, "%s", path);
                        break;
                    case 's':
                        res = ydb_path_sync(datablock, "%s", path);
                        break;
                    default:
                        break;
                    }
                    if (YDB_FAILED(res))
                        printf("\nydb error: %s (%s)\n", ydb_res_str(res), path);
                    n = ydb_next(n);
                    if (n)
                        fprintf(stdout, "\n");
                }
            }
        }

        if (strcmp(ydb_path_read(config, "/config/summary"), "true") == 0)
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
    if (verbose)
        ylog_level = YLOG_ERROR;
    ydb_close(config);
#ifdef ENABLED_PLOG
    plog_cleanup();
#endif
    if (res)
        return 1;
    return 0;
}
