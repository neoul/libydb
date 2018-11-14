#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <yaml.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>

// epoll
#include <sys/epoll.h>

// true/false
#include <stdbool.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "yalloc.h"
#include "ytree.h"
#include "ylist.h"
#include "yarray.h"
#include "ytrie.h"

#include "ydb.h"
#include "ynode.h"

int ydb_logger_example(
    int severity, const char *func, int line, const char *format, ...)
{
    int len = -1;
    va_list args;
    FILE *fp = NULL;
    switch (severity)
    {
    case YDB_LOG_DEBUG:
        fp = stdout;
        fprintf(fp, "** ydb::debug::%s:%d: ", func, line);
        break;
    case YDB_LOG_INOUT:
        fp = stdout;
        fprintf(fp, "** ydb::inout:%s:%d: ", func, line);
        break;
    case YDB_LOG_INFO:
        fp = stdout;
        fprintf(fp, "** ydb::info::%s:%d: ", func, line);
        break;
    case YDB_LOG_WARN:
        fp = stdout;
        fprintf(fp, "** ydb::warn::%s:%d: ", func, line);
        break;
    case YDB_LOG_ERR:
        fp = stdout;
        fprintf(fp, "** ydb::error:%s:%d: ", func, line);
        break;
    case YDB_LOG_CRI:
        fp = stdout;
        fprintf(fp, "** ydb::critical:%s:%d: ", func, line);
        break;
    default:
        return 0;
    }
    va_start(args, format);
    len = vfprintf(fp, format, args);
    va_end(args);
    return len;
}

unsigned int ydb_log_severity = YDB_LOG_ERR;
ydb_log_func ydb_logger = ydb_logger_example;
int ydb_log_register(ydb_log_func func)
{
    ydb_logger = func;
    return 0;
}

#define YDB_ASSERT(state, caused_res)                                        \
    do                                                                       \
    {                                                                        \
        if (state)                                                           \
        {                                                                    \
            ydb_logger(YDB_LOG_ERR, __func__, __LINE__, "ASSERT '%s': %s\n", \
                       #state, ydb_res_str[caused_res]);                     \
            assert(!(state));                                                \
        }                                                                    \
    } while (0)

#define YDB_FAIL_ERRNO(state, caused_res, error)                               \
    do                                                                         \
    {                                                                          \
        if (state)                                                             \
        {                                                                      \
            res = caused_res;                                                  \
            if (ydb_log_severity >= (YDB_LOG_ERR))                             \
            {                                                                  \
                ydb_logger(YDB_LOG_ERR, __func__, __LINE__, "'%s': %s (%s)\n", \
                           #state, ydb_res_str[caused_res], strerror(errno));  \
            }                                                                  \
            goto failed;                                                       \
        }                                                                      \
    } while (0)

#define YDB_FAIL(state, caused_res)                                       \
    do                                                                    \
    {                                                                     \
        if (state)                                                        \
        {                                                                 \
            res = caused_res;                                             \
            if (ydb_log_severity >= (YDB_LOG_ERR))                        \
            {                                                             \
                ydb_logger(YDB_LOG_ERR, __func__, __LINE__, "'%s': %s\n", \
                           #state, ydb_res_str[caused_res]);              \
            }                                                             \
            goto failed;                                                  \
        }                                                                 \
    } while (0)

#define ydb_log(severity, format, ...)                                           \
    do                                                                           \
    {                                                                            \
        if (ydb_log_severity < (severity))                                       \
            break;                                                               \
        ydb_logger(severity, (__FUNCTION__), (__LINE__), format, ##__VA_ARGS__); \
    } while (0)

#define ydb_log_debug(format, ...) ydb_log(YDB_LOG_DEBUG, format, ##__VA_ARGS__)
#define ydb_log_inout() ydb_log(YDB_LOG_INOUT, "\n")
#define ydb_log_in() ydb_log(YDB_LOG_INOUT, "{{ ------\n")
#define ydb_log_out() ydb_log(YDB_LOG_INOUT, "}}\n")
#define ydb_log_info(format, ...) ydb_log(YDB_LOG_INFO, format, ##__VA_ARGS__)
#define ydb_log_warn(format, ...) ydb_log(YDB_LOG_WARN, format, ##__VA_ARGS__)
#define ydb_log_error(format, ...) ydb_log(YDB_LOG_ERR, format, ##__VA_ARGS__)
#define YDB_LOGGING_DEBUG (ydb_log_severity >= YDB_LOG_DEBUG)
#define YDB_LOGGING_INFO (ydb_log_severity >= YDB_LOG_INFO)

#define SET_FLAG(flag, v) ((flag) = ((flag) | (v)))
#define UNSET_FLAG(flag, v) ((flag) = ((flag) & (~v)))
#define IS_SET(flag, v) ((flag) & (v))

#define CLEAR_BUF(buf, buflen) \
    do                         \
    {                          \
        if (buf)               \
            free(buf);         \
        buf = NULL;            \
        buflen = 0;            \
    } while (0)

#define YDB_ERR_NAME(NAME) #NAME
char *ydb_res_str[] =
    {
        YDB_ERR_NAME(YDB_OK),
        YDB_ERR_NAME(YDB_ERR),
        YDB_ERR_NAME(YDB_E_SYSTEM_FAILED),
        YDB_ERR_NAME(YDB_E_STREAM_FAILED),
        YDB_ERR_NAME(YDB_E_PERSISTENCY_ERR),
        YDB_ERR_NAME(YDB_E_INVALID_ARGS),
        YDB_ERR_NAME(YDB_E_TYPE_ERR),
        YDB_ERR_NAME(YDB_E_INVALID_PARENT),
        YDB_ERR_NAME(YDB_E_NO_ENTRY),
        YDB_ERR_NAME(YDB_E_MEM_ALLOC),
        YDB_ERR_NAME(YDB_E_FULL_BUF),
        YDB_ERR_NAME(YDB_E_INVALID_YAML_TOKEN),
        YDB_ERR_NAME(YDB_E_YAML_INIT_FAILED),
        YDB_ERR_NAME(YDB_E_YAML_PARSING_FAILED),
        YDB_ERR_NAME(YDB_E_MERGE_FAILED),
        YDB_ERR_NAME(YDB_E_DELETE_FAILED),
        YDB_ERR_NAME(YDB_E_INVALID_MSG),
        YDB_ERR_NAME(YDB_E_ENTRY_EXISTS),
        YDB_ERR_NAME(YDB_E_NO_CONN),
        YDB_ERR_NAME(YDB_E_CONN_FAILED),
        YDB_ERR_NAME(YDB_E_CONN_CLOSED),
        YDB_ERR_NAME(YDB_E_FUNC),
        YDB_ERR_NAME(YDB_W_UPDATED),
};

typedef struct _yconn yconn;

#define YCONN_ROLE_PUBLISHER 0x0001
#define YCONN_WRITABLE 0x0002
#define YCONN_UNSUBSCRIBE 0x0004
#define YCONN_RECONNECT 0x0008
#define YCONN_SYNC 0x0010
#define YCONN_UNREADABLE 0x0020
#define YCONN_MAJOR_CONN 0x0040
#define YCONN_FLAGS_MASK 0x00ff

#define YCONN_TYPE_UNIX 0x0100
#define YCONN_TYPE_INET 0x0200
#define YCONN_TYPE_FIFO 0x0400
#define YCONN_TYPE_FILE 0x0800
#define YCONN_TYPE_MASK 0xff00

#define STATUS_SERVER 0x010000
#define STATUS_CLIENT 0x020000
#define STATUS_COND_CLIENT 0x040000 // connected client
#define STATUS_DISCONNECT 0x080000
#define STATUS_MASK 0xff0000

typedef enum
{
    YOP_NONE,
    YOP_INIT,
    YOP_MERGE,
    YOP_DELETE,
    YOP_SYNC,
} yconn_op;

char *yconn_op_str[] = {
    "none",
    "init",
    "merge",
    "delete",
    "sync"};

typedef enum
{
    YMSG_NONE,
    YMSG_REQUEST,
    YMSG_RESPONSE,
    YMSG_RESP_FAILED,
    YMSG_PUBLISH,
} ymsg_type;

char *ymsg_str[] = {
    "none",
    "request",
    "resp(ok)",
    "resp(failed)",
    "pubish",
};

typedef ydb_res (*yconn_func_send)(yconn *conn, yconn_op op, ymsg_type type, char *data, size_t datalen);
typedef ydb_res (*yconn_func_recv)(
    yconn *conn, yconn_op *op, ymsg_type *type,
    unsigned int *flags, char **data, size_t *datalen, int *next);
typedef int (*yconn_func_accept)(yconn *conn, yconn *client); // return fd;

typedef ydb_res (*yconn_func_init)(yconn *conn);
typedef void (*yconn_func_deinit)(yconn *conn);

struct _yconn
{
    ydb *db;
    char *address;
    unsigned int flags;
    int fd;
    yconn_func_init func_init;
    yconn_func_recv func_recv;
    yconn_func_send func_send;
    yconn_func_accept func_accept;
    yconn_func_deinit func_deinit;
    void *head;
};

static void yconn_print(yconn *conn, const char *func, int line, char *state, bool simple);
#define YCONN_INFO(conn, state) \
    yconn_print(conn, __func__, __LINE__, state, false)
#define YCONN_SIMPLE_INFO(conn) \
    yconn_print(conn, __func__, __LINE__, NULL, true)

static unsigned int yconn_flags(char *address, char *flagstr);
static yconn *yconn_new(char *address, unsigned int flags);
static void yconn_free(yconn *conn);
static void yconn_close(yconn *conn);
static ydb_res yconn_open(char *addr, char *flags, ydb *datablock);
static ydb_res yconn_reopen(yconn *conn, ydb *datablock);
static int yconn_accept(yconn *conn);
static yconn *yconn_get(char *address);
static ydb *yconn_detach(yconn *conn);
static ydb_res yconn_attach(yconn *conn, ydb *datablock);
static ydb_res yconn_request(yconn *conn, yconn_op op, char *buf, size_t buflen);
static ydb_res yconn_response(yconn *conn, yconn_op op, bool done, char *buf, size_t buflen);
static ydb_res yconn_publish(yconn *src_target, ydb *datablock, yconn_op op, char *buf, size_t buflen);
static ydb_res yconn_sync(yconn *src, ydb *datablock);
static ydb_res yconn_init(yconn *conn);
static ydb_res yconn_merge(yconn *conn, char *buf, size_t buflen);
static ydb_res yconn_delete(yconn *conn, char *buf, size_t buflen);
static ydb_res yconn_recv(yconn *conn, yconn_op *op, ymsg_type *type, int *next);

#define yconn_errno(conn, res)            \
    ydb_log(YDB_LOG_ERR, "%s: %s (%s)\n", \
            (conn)->address, ydb_res_str[res], strerror(errno));

#define yconn_error(conn, res)       \
    ydb_log(YDB_LOG_ERR, "%s: %s\n", \
            (conn)->address, ydb_res_str[res]);

struct _ydb
{
    char *name;
    ynode *top;
    ytrie *updater;
    ytree *conn;
    ylist *disconn;
    int epollfd;
    int epollcount;
    int synccount;
};

ytrie *ydb_pool;
ytrie *yconn_pool;

int yconn_cmp(int *fd1, int *fd2)
{
    if (*fd1 < *fd2)
        return -1;
    else if (*fd1 > *fd2)
        return 1;
    else
        return 0;
}

static ydb_res ypool_create()
{
    if (!ydb_pool)
    {
        ydb_pool = ytrie_create();
        if (!ydb_pool)
        {
            return YDB_E_MEM_ALLOC;
        }
    }
    if (!yconn_pool)
    {
        yconn_pool = ytrie_create();
        if (!yconn_pool)
        {
            return YDB_E_MEM_ALLOC;
        }
    }
    return YDB_OK;
}

static void ypool_destroy()
{
    if (ydb_pool && ytrie_size(ydb_pool) <= 0)
    {
        ytrie_destroy(ydb_pool);
        ydb_pool = NULL;
    }
    if (yconn_pool && ytrie_size(yconn_pool) <= 0)
    {
        ytrie_destroy(yconn_pool);
        yconn_pool = NULL;
    }
}

static void ydb_print(ydb *datablock, const char *func, int line, char *state)
{
    if (!datablock || !YDB_LOGGING_INFO)
        return;
    ydb_logger(YDB_LOG_INFO, func, line, "%s ydb:\n", state ? state : "");
    ydb_logger(YDB_LOG_INFO, func, line, "  name: %s\n", datablock->name);
    ydb_logger(YDB_LOG_INFO, func, line, "  epollfd: %d\n", datablock->epollfd);
    ydb_logger(YDB_LOG_INFO, func, line, "  epollcount: %d\n", datablock->epollcount);
    ydb_logger(YDB_LOG_INFO, func, line, "  synccount: %d\n", datablock->synccount);
    if (!ylist_empty(datablock->disconn))
    {
        ydb_logger(YDB_LOG_INFO, func, line,
                   "  disconn: %d\n", ylist_size(datablock->disconn));
    }
    if (ytree_size(datablock->conn) > 0)
    {
        ydb_logger(YDB_LOG_INFO, func, line,
                   "  conn: %d\n", ytree_size(datablock->conn));
    }
    if (ytrie_size(datablock->updater) > 0)
    {
        ydb_logger(YDB_LOG_INFO, func, line,
                   "  write hooks: %d\n", ytrie_size(datablock->updater));
    }
}
#define YDB_INFO(conn, state) ydb_print((conn), __func__, __LINE__, (state))

// open local ydb (yaml data block)
ydb *ydb_open(char *name)
{
    ydb_res res = YDB_OK;
    ydb *datablock = NULL;
    int namelen;
    ydb_log_inout();
    res = (ydb_res)res;
    YDB_FAIL(!name, YDB_E_INVALID_ARGS);
    YDB_FAIL(ypool_create(), YDB_E_SYSTEM_FAILED);
    namelen = strlen(name);
    datablock = ytrie_search(ydb_pool, name, namelen);
    if (datablock)
        return datablock;
    ydb_log_in();
    datablock = malloc(sizeof(ydb));
    YDB_FAIL(!datablock, YDB_E_SYSTEM_FAILED);
    memset(datablock, 0x0, sizeof(ydb));
    datablock->epollfd = -1;

    datablock->name = ystrdup(name);
    YDB_FAIL(!datablock->name, YDB_E_SYSTEM_FAILED);
    datablock->conn = ytree_create((ytree_cmp)yconn_cmp, NULL);
    YDB_FAIL(!datablock->conn, YDB_E_SYSTEM_FAILED);
    datablock->top = ynode_create_path(name, NULL, NULL);
    YDB_FAIL(!datablock->top, YDB_E_SYSTEM_FAILED);
    datablock->disconn = ylist_create();
    YDB_FAIL(!datablock->disconn, YDB_E_SYSTEM_FAILED);
    datablock->updater = ytrie_create();
    YDB_FAIL(!datablock->updater, YDB_E_SYSTEM_FAILED);
    ytrie_insert(ydb_pool, datablock->name, namelen, datablock);
    YDB_INFO(datablock, "opened");
    ydb_log_out();
    return datablock;
failed:
    ydb_close(datablock);
    ydb_log_out();
    return NULL;
}

// address: use the unix socket named to the YDB name if null
//          us://unix-socket-name (unix socket)
//          uss://unix-socket-name (hidden unix socket)
//          tcp://ipaddr:port (tcp)
//          fifo://named-fifo-input,named-fifo-output
// flags: pub(publisher)/sub(subscriber)
//        w(writable): connect to a remote to write.
//        u(unsubscribe): disable the subscription of the data change
//        r(reconnect mode): retry the connection in ydb_serve()
//        s(sync-before-read mode): request the update of the YDB before ydb_read()
// e.g. ydb_connect(db, "us:///tmp/ydb_channel", "sub:w")
ydb_res ydb_connect(ydb *datablock, char *addr, char *flags)
{
    ydb_res res = YDB_OK;
    yconn *conn = NULL;
    char _addr[256];
    ydb_log_in();
    YDB_FAIL(!datablock || !flags, YDB_E_INVALID_ARGS);
    if (datablock->epollfd < 0)
    {
        datablock->epollfd = epoll_create(YDB_CONN_MAX);
        ydb_log_info("open epollfd(%d)\n", datablock->epollfd);
    }
    YDB_FAIL(datablock->epollfd < 0, YDB_E_SYSTEM_FAILED);

    if (!addr)
    {
        snprintf(_addr, sizeof(_addr), "uss://%s", datablock->name);
        addr = _addr;
    }
    res = yconn_open(addr, flags, datablock);
    YDB_FAIL(res, res);
    ydb_log_out();
    return res;
failed:
    yconn_close(conn);
    if (datablock->epollfd > 0)
    {
        if (ylist_empty(datablock->disconn) &&
            ytree_size(datablock->conn) <= 0)
        {
            close(datablock->epollfd);
            datablock->epollfd = -1;
        }
    }
    ydb_log_out();
    return res;
}

ydb_res ydb_reconnect(ydb *datablock, char *addr, char *flags)
{
    ydb_res res = YDB_OK;
    yconn *reconn = NULL;
    char _addr[256];
    unsigned int conn_flags = 0;
    ydb_log_in();
    YDB_FAIL(!datablock || !flags, YDB_E_INVALID_ARGS);
    if (datablock->epollfd < 0)
    {
        datablock->epollfd = epoll_create(YDB_CONN_MAX);
        ydb_log_info("open epollfd(%d)\n", datablock->epollfd);
    }
    YDB_FAIL(datablock->epollfd < 0, YDB_E_SYSTEM_FAILED);

    if (!addr)
    {
        snprintf(_addr, sizeof(_addr), "uss://%s", datablock->name);
        addr = _addr;
    }
    conn_flags = yconn_flags(addr, flags);
    YDB_FAIL(!conn_flags, YDB_E_INVALID_ARGS);
    reconn = yconn_get(addr);
    if (reconn)
        yconn_close(reconn);
    res = ydb_connect(datablock, addr, flags);
    YDB_FAIL(res, res);
    ydb_log_out();
    return res;
failed:
    if (datablock->epollfd > 0)
    {
        if (ylist_empty(datablock->disconn) &&
            ytree_size(datablock->conn) <= 0)
        {
            close(datablock->epollfd);
            datablock->epollfd = -1;
        }
    }
    ydb_log_out();
    return res;
}

ydb_res ydb_disconnect(ydb *datablock, char *addr)
{
    ydb_res res = YDB_E_NO_ENTRY;
    yconn *disconn = NULL;
    char _addr[256];
    ydb_log_in();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    if (!addr)
    {
        snprintf(_addr, sizeof(_addr), "uss://%s", datablock->name);
        addr = _addr;
    }
    disconn = yconn_get(addr);
    if (disconn)
    {
        yconn_close(disconn);
        res = YDB_OK;
    }
    if (datablock->epollfd > 0)
    {
        if (ylist_empty(datablock->disconn) &&
            ytree_size(datablock->conn) <= 0)
        {
            ydb_log_info("close epollfd(%d)\n", datablock->epollfd);
            close(datablock->epollfd);
            datablock->epollfd = -1;
        }
    }

    ydb_log_out();
    return res;
failed:
    ydb_log_out();
    return res;
}

// Clear all data in YAML DataBlock
ydb_res ydb_clear(ydb *datablock)
{
    ydb_res res = YDB_OK;
    ynode_log *log;
    size_t buflen = 0;
    char *buf = NULL;
    ynode *n;
    ydb_log_in();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    log = ynode_log_open(datablock->top, NULL);
    n = ynode_down(datablock->top);
    while (n)
    {
        ynode_delete(n, log);
        n = ynode_down(datablock->top);
    }
    ynode_log_close(log, &buf, &buflen);
    yconn_publish(NULL, datablock, YOP_DELETE, buf, buflen);
failed:
    CLEAR_BUF(buf, buflen);
    ydb_log_out();
    return res;
}

// Close YAML Datablock
void ydb_close(ydb *datablock)
{
    ydb_log_in();
    if (datablock)
    {
        YDB_INFO(datablock, "closed");
        ytrie_delete(ydb_pool, datablock->name, strlen(datablock->name));
        if (datablock->epollfd > 0)
            close(datablock->epollfd);
        if (datablock->disconn)
            ylist_destroy_custom(datablock->disconn, (user_free)yconn_free);
        if (datablock->conn)
            ytree_destroy_custom(datablock->conn, (user_free)yconn_free);
        if (datablock->updater)
            ytrie_destroy_custom(datablock->updater, (user_free)free);
        if (datablock->top)
            ynode_delete(ynode_top(datablock->top), NULL);
        if (datablock->name)
            yfree(datablock->name);
        if (datablock->epollfd > 0)
            close(datablock->epollfd);
        free(datablock);
    }
    ypool_destroy();
    ydb_log_out();
}

ydb *ydb_get(char *name_and_path, ynode **node)
{
    ydb *datablock;
    int mlen = 0;
    int slen = strlen(name_and_path);
    datablock = ytrie_best_match(ydb_pool, name_and_path, slen, &mlen);
    if (datablock && node)
    {
        if (mlen < slen)
            *node = ynode_search(datablock->top, name_and_path + mlen);
    }
    return datablock;
}

// return the new string consisting of the YDB name and the path to the iter.
// the return string must be free.
char *ydb_name_and_path(ynode *node, int *pathlen)
{
    return ynode_path(node, YDB_LEVEL_MAX, pathlen);
}

char *ydb_name(ydb *datablock)
{
    return datablock->name;
}

// return the node in the path of the yaml data block.
ynode *ydb_search(ydb *datablock, char *path)
{
    return ynode_search(datablock->top, path);
}

// return the path of the node. (the path must be free.)
char *ydb_path(ydb *datablock, ynode *node, int *pathlen)
{
    return ynode_path(node, ynode_level(datablock->top, node), pathlen);
}

// return the path of the node. (the path must be free.)
char *ydb_path_and_value(ydb *datablock, ynode *node, int *pathlen)
{
    return ynode_path_and_val(node, ynode_level(datablock->top, node), pathlen);
}

// return the top node of the yaml data block.
ynode *ydb_top(ydb *datablock)
{
    if (datablock)
        return datablock->top;
    return NULL;
}

// return the root node of the yaml data block.
ynode *ydb_root(ydb *datablock)
{
    return ynode_top(datablock->top);
}

// return 1 if ydb_iter is empty.
int ydb_empty(ynode *node)
{
    return ynode_empty(node);
}

// return the parent node of the node.
ynode *ydb_up(ynode *node)
{
    return ynode_up(node);
}

// return the first child node of the node.
ynode *ydb_down(ynode *node)
{
    return ynode_down(node);
}

// return the previous sibling node of the node.
ynode *ydb_prev(ynode *node)
{
    return ynode_prev(node);
}

// return the next sibling node of the node.
ynode *ydb_next(ynode *node)
{
    return ynode_next(node);
}

// return the first sibling node of the node.
ynode *ydb_first(ynode *node)
{
    return ynode_first(node);
}

// return the last sibling node of the node.
ynode *ydb_last(ynode *node)
{
    return ynode_last(node);
}

// return node type
int ydb_type(ynode *node)
{
    return ynode_type(node);
}

// return node value if that is a leaf.
char *ydb_value(ynode *node)
{
    return ynode_value(node);
}

// return node key if that has a hash key.
char *ydb_key(ynode *node)
{
    return ynode_key(node);
}

// return node index if the nodes' parent is a list.
int ydb_index(ynode *node)
{
    return ynode_index(node);
}

ydb_res ydb_parse(ydb *datablock, FILE *fp)
{
    ydb_res res = YDB_OK;
    char *buf = NULL;
    size_t buflen = 0;
    ynode *src = NULL;
    ydb_log_in();
    res = ynode_scanf_from_fp(fp, &src);
    YDB_FAIL(res, res);
    if (src)
    {
        ynode *top;
        ynode_log *log = NULL;
        log = ynode_log_open(datablock->top, NULL);
        top = ynode_merge(datablock->top, src, log);
        ynode_log_close(log, &buf, &buflen);
        if (top)
        {
            datablock->top = top;
            yconn_publish(NULL, datablock, YOP_MERGE, buf, buflen);
        }
        else
        {
            YDB_FAIL(YDB_E_MERGE_FAILED, YDB_E_MERGE_FAILED);
        }
    }
failed:
    CLEAR_BUF(buf, buflen);
    ynode_remove(src);
    ydb_log_out();
    return res;
}

ydb_res ydb_parses(ydb *datablock, char *buf, size_t buflen)
{
    ydb_res res = YDB_OK;
    ynode *src = NULL;
    ydb_log_in();
    res = ynode_scanf_from_buf(buf, buflen, 0, &src);
    YDB_FAIL(res, res);
    if (src)
    {
        ynode *top;
        ynode_log *log = NULL;
        log = ynode_log_open(datablock->top, NULL);
        top = ynode_merge(datablock->top, src, log);
        ynode_log_close(log, &buf, &buflen);
        if (top)
        {
            datablock->top = top;
            yconn_publish(NULL, datablock, YOP_MERGE, buf, buflen);
        }
        else
        {
            YDB_FAIL(YDB_E_MERGE_FAILED, YDB_E_MERGE_FAILED);
        }
    }
failed:
    CLEAR_BUF(buf, buflen);
    ynode_remove(src);
    ydb_log_out();
    return res;
}

int ydb_dump(ydb *datablock, FILE *fp)
{
    if (!datablock)
        return -1;
    return ynode_printf_to_fp(fp, datablock->top, 1, YDB_LEVEL_MAX);
}

int ydb_dump_debug(ydb *datablock, FILE *fp)
{
    if (!datablock)
        return -1;
    ynode_dump_to_fp(fp, datablock->top, 1, YDB_LEVEL_MAX);
    return 0;
}

int ydb_dumps(ydb *datablock, char **buf, size_t *buflen)
{
    FILE *fp;
    if (!datablock)
        return -1;
    *buf = NULL;
    *buflen = 0;
    fp = open_memstream(buf, buflen);
    if (fp)
    {
        int n = ynode_printf_to_fp(fp, datablock->top, 1, YDB_LEVEL_MAX);
        fclose(fp);
        return n;
    }
    else
        return -1;
}

// update ydb using the input string
ydb_res ydb_write(ydb *datablock, const char *format, ...)
{
    ydb_res res = YDB_OK;
    ynode *src = NULL;

    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;

    ydb_log_in();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    fp = open_memstream(&buf, &buflen);
    YDB_FAIL(!fp, YDB_E_STREAM_FAILED);

    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fclose(fp);

    {
        ynode *top = NULL;
        ynode_log *log = NULL;
        res = ynode_scanf_from_buf(buf, buflen, 0, &src);
        YDB_FAIL(res || !src, res);
        CLEAR_BUF(buf, buflen);
        log = ynode_log_open(datablock->top, NULL);
        top = ynode_merge(datablock->top, src, log);
        ynode_log_close(log, &buf, &buflen);
        YDB_FAIL(!top, YDB_E_MERGE_FAILED);
        datablock->top = top;
        yconn_publish(NULL, datablock, YOP_MERGE, buf, buflen);
    }
failed:
    CLEAR_BUF(buf, buflen);
    ynode_remove(src);
    ydb_log_out();
    return res;
}

struct ydb_delete_data
{
    ynode_log *log;
    ynode *node;
};

static ydb_res ydb_delete_sub(ynode *cur, void *addition)
{
    struct ydb_delete_data *pddata = (void *)addition;
    ynode *n = pddata->node;
    ynode *target = ynode_lookup(n, cur, 1);
    if (target)
        ynode_delete(target, pddata->log);
    return YDB_OK;
}

// delete ydb using the input string
ydb_res ydb_delete(ydb *datablock, const char *format, ...)
{
    ydb_res res = YDB_OK;
    ynode *src = NULL;

    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;

    ydb_log_in();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    fp = open_memstream(&buf, &buflen);
    YDB_FAIL(!fp, YDB_E_STREAM_FAILED);

    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fclose(fp);

    {
        unsigned int flags;
        struct ydb_delete_data ddata;
        res = ynode_scanf_from_buf(buf, buflen, 0, &src);
        YDB_FAIL(res || !src, res);
        CLEAR_BUF(buf, buflen);
        ddata.log = ynode_log_open(datablock->top, NULL);
        ddata.node = datablock->top;
        flags = YNODE_LEAF_FIRST | YNODE_VAL_ONLY;
        res = ynode_traverse(src, ydb_delete_sub, &ddata, flags);
        ynode_log_close(ddata.log, &buf, &buflen);
        if (res)
            yconn_publish(NULL, datablock, YOP_DELETE, buf, buflen);
    }
failed:
    CLEAR_BUF(buf, buflen);
    ynode_remove(src);
    ydb_log_out();
    return res;
}

struct readhook
{
    union {
        ydb_read_hook hook;
        ydb_read_hook0 hook0;
        ydb_read_hook1 hook1;
        ydb_read_hook2 hook2;
        ydb_read_hook3 hook3;
        ydb_read_hook4 hook4;
    };
    int num;
    void *user[];
};

struct ydb_update_params
{
    ydb *datablock;
    bool updated;
};

static ydb_res ydb_update_sub(ynode *cur, void *addition)
{
    ydb_res res = YDB_OK;
    struct ydb_update_params *params = addition;
    ydb *datablock = params->datablock;
    struct readhook *rhook = NULL;
    int pathlen = 0;
    char *path = ydb_path(datablock, cur, &pathlen);
    if (path && pathlen > 0)
    {
        // int matched_len = 0;
        // hook = ytrie_best_match(datablock->updater, path, pathlen, &matched_len);
        rhook = ytrie_search(datablock->updater, path, pathlen);
        ydb_log_info("hook %s %s (%d)\n", rhook ? "found" : "not found", path, pathlen);
    }
    if (rhook)
    {
        FILE *fp;
        char *buf = NULL;
        size_t buflen = 0;
        ynode *src = NULL;
        ynode *top = NULL;
        ynode_log *log = NULL;
        fp = open_memstream(&buf, &buflen);
        if (fp)
        {
            switch (rhook->num)
            {
            case 0:
                res = rhook->hook0(datablock, path, fp);
                break;
            case 1:
                res = rhook->hook1(datablock, path, fp, rhook->user[0]);
                break;
            case 2:
                res = rhook->hook2(
                    datablock, path, fp, rhook->user[0], rhook->user[1]);
                break;
            case 3:
                res = rhook->hook3(
                    datablock, path, fp, rhook->user[0], rhook->user[1], rhook->user[2]);
                break;
            case 4:
                res = rhook->hook4(
                    datablock, path, fp, rhook->user[0], rhook->user[1], rhook->user[2], rhook->user[3]);
                break;
            default:
                break;
            }
            fclose(fp);
        }
        res = ynode_scanf_from_buf(buf, buflen, 0, &src);
        CLEAR_BUF(buf, buflen);
        if (res)
        {
            ynode_remove(src);
            goto done;
        }
        if (!src)
            goto done;

        log = ynode_log_open(datablock->top, NULL);
        top = ynode_merge(datablock->top, src, log);
        ynode_log_close(log, &buf, &buflen);
        ynode_remove(src);
        if (top)
        {
            datablock->top = top;
            yconn_publish(NULL, datablock, YOP_MERGE, buf, buflen);
            params->updated = true;
        }
        else
            res = YDB_E_MERGE_FAILED;
        CLEAR_BUF(buf, buflen);
    }
done:
    if (path)
        free(path);
    return res;
}

ydb_res ydb_update(ydb *datablock, ynode *target)
{
    ydb_res res = YDB_OK;
    struct ydb_update_params params;
    if (datablock && ytrie_size(datablock->updater) > 0)
    {
        params.datablock = datablock;
        params.updated = false;
        res = ynode_traverse(target, ydb_update_sub, &params, YNODE_LEAF_FIRST);
    }
    if (!res)
        return YDB_W_UPDATED;
    return res;
}

ydb_res ydb_read_hook_add(ydb *datablock, char *path, ydb_read_hook func, int num, ...)
{
    ydb_res res;
    int pathlen;
    struct readhook *rhook;
    ydb_log_in();
    YDB_FAIL(!datablock || !func || !path || num < 0, YDB_E_INVALID_ARGS);
    YDB_FAIL(num > 4 || num < 0, YDB_E_INVALID_ARGS);
    pathlen = strlen(path);
    rhook = ytrie_search(datablock->updater, path, pathlen);
    // YDB_FAIL(rhook, YDB_E_ENTRY_EXISTS);
    if (!rhook)
        rhook = malloc(sizeof(struct readhook) + sizeof(void *) * num);
    YDB_FAIL(!rhook, YDB_E_NO_ENTRY);
    rhook->hook = func;
    rhook->num = num;
    {
        int i;
        va_list ap;
        va_start(ap, num);
        ydb_log_debug("user total = %d\n", num);
        for (i = 0; i < num; i++)
        {
            void *p = va_arg(ap, void *);
            rhook->user[i] = p;
            ydb_log_debug("p=%p\n", p);
        }
        va_end(ap);
    }
    ytrie_insert(datablock->updater, path, pathlen, rhook);
    return YDB_OK;
failed:
    ydb_log_out();
    return res;
}

void ydb_read_hook_delete(ydb *datablock, char *path)
{
    int pathlen;
    struct readhook *rhook;
    ydb_log_in();
    if (!datablock || !path)
    {
        ydb_log_out();
        return;
    }
    pathlen = strlen(path);
    rhook = ytrie_delete(datablock->updater, path, pathlen);
    ydb_log_out();
    free(rhook);
}

struct ydb_read_data
{
    ydb *datablock;
    yarray *vararray;
    int vartotal;
    int varnum;
};

static ydb_res ydb_read_sub(ynode *cur, void *addition)
{
    struct ydb_read_data *data = addition;
    char *value = ynode_value(cur);

    if (value && strncmp(value, "+", 1) == 0)
    {
        ynode *n = ynode_lookup(data->datablock->top, cur, 0);
        if (n)
        {
            int index = atoi(value);
            void *p = yarray_data(data->vararray, index);
            ydb_log_debug("index=%d p=%p\n", index, p);
            if (YDB_LOGGING_DEBUG)
            {
                char buf[512];
                ynode_dump_to_buf(buf, sizeof(buf), n, 0, 0);
                ydb_log_debug("%s", buf);
                ynode_dump_to_buf(buf, sizeof(buf), cur, 0, 0);
                ydb_log_debug("%s", buf);
            }
            sscanf(ynode_value(n), &(value[4]), p);
            data->varnum++;
        }
        else
        {
            if (YDB_LOGGING_DEBUG)
            {
                char *path = ynode_path(cur, YDB_LEVEL_MAX, NULL);
                ydb_log_debug("no data for (%s)\n", path);
                free(path);
            }
        }
    }
    return YDB_OK;
}

ydb_res ynode_scan(FILE *fp, char *buf, int buflen, int origin, ynode **n, int *queryform);

// read the date from ydb as the scanf()
int ydb_read(ydb *datablock, const char *format, ...)
{
    ydb_res res = YDB_OK;
    struct ydb_read_data data;
    ynode *src = NULL;
    unsigned int flags;
    int ap_num = 0;

    ydb_log_in();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    res = ynode_scan(NULL, (char *)format, strlen(format), 0, &src, &ap_num);
    YDB_FAIL(res, res);

    if (ap_num <= 0 || !src)
    {
        ynode_remove(src);
        ydb_log_out();
        return 0;
    }

    ydb_update(datablock, src);

    data.vararray = yarray_create(16);
    data.vartotal = ap_num;
    data.varnum = 0;
    data.datablock = datablock;

    {
        va_list ap;
        va_start(ap, format);
        ydb_log_debug("var total = %d\n", ap_num);
        do
        {
            void *p = va_arg(ap, void *);
            yarray_push_back(data.vararray, p);
            ydb_log_debug("p=%p\n", p);
            ap_num--;
        } while (ap_num > 0);
        va_end(ap);
    }

    flags = YNODE_LEAF_FIRST | YNODE_VAL_ONLY;
    if (datablock->synccount > 0)
    {
        res = yconn_sync(NULL, datablock);
        YDB_FAIL(res, res);
    }
    res = ynode_traverse(src, ydb_read_sub, &data, flags);
    YDB_FAIL(res, res);
    ydb_log_debug("var read = %d\n", data.varnum);
failed:
    yarray_destroy(data.vararray);
    ynode_remove(src);
    ydb_log_out();
    if (res)
        return -1;
    return data.varnum;
}

// update the ydb using input path and value
// ydb_path_write(datablock, "/path/to/update=%d", value)
ydb_res ydb_path_write(ydb *datablock, const char *format, ...)
{
    ydb_res res = YDB_OK;
    ynode *src = NULL;
    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;

    ydb_log_in();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    fp = open_memstream(&buf, &buflen);
    YDB_FAIL(!fp, YDB_E_STREAM_FAILED);

    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fclose(fp);

    {
        char *rbuf = NULL;
        size_t rbuflen = 0;
        ynode_log *log = NULL;
        log = ynode_log_open(datablock->top, NULL);
        src = ynode_create_path(buf, datablock->top, log);
        ynode_log_close(log, &rbuf, &rbuflen);
        if (rbuf)
        {
            if (src)
                yconn_publish(NULL, datablock, YOP_MERGE, rbuf, rbuflen);
            free(rbuf);
        }
    }
    YDB_FAIL(!src, YDB_E_MERGE_FAILED);

failed:
    CLEAR_BUF(buf, buflen);
    ydb_log_out();
    return res;
}

// delete the ydb using input path
// ydb_path_delete(datablock, "/path/to/update\n")
ydb_res ydb_path_delete(ydb *datablock, const char *format, ...)
{
    ydb_res res = YDB_OK;
    ynode *target = NULL;
    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;

    ydb_log_in();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    fp = open_memstream(&buf, &buflen);
    YDB_FAIL(!fp, YDB_E_STREAM_FAILED);

    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fclose(fp);

    {
        char *rbuf = NULL;
        size_t rbuflen = 0;
        ynode_log *log = NULL;
        log = ynode_log_open(datablock->top, NULL);
        target = ynode_search(datablock->top, buf);
        if (target)
            ynode_delete(target, log);
        ynode_log_close(log, &rbuf, &rbuflen);
        if (rbuf)
        {
            if (target)
                yconn_publish(NULL, datablock, YOP_DELETE, rbuf, rbuflen);
            free(rbuf);
        }
    }
    YDB_FAIL(!target, YDB_E_DELETE_FAILED);
failed:
    CLEAR_BUF(buf, buflen);
    ydb_log_out();
    return res;
}

// read the value from ydb using input path
// char *value = ydb_path_read(datablock, "/path/to/update")
char *ydb_path_read(ydb *datablock, const char *format, ...)
{
    ydb_res res = YDB_OK;
    ynode *src = NULL;
    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;

    ydb_log_in();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    fp = open_memstream(&buf, &buflen);
    YDB_FAIL(!fp, YDB_E_STREAM_FAILED);

    {
        va_list args;
        va_start(args, format);
        vfprintf(fp, format, args);
        va_end(args);
        fclose(fp);
        if (datablock->synccount > 0)
        {
            res = yconn_sync(NULL, datablock);
            YDB_FAIL(res, res);
        }
        src = ynode_search(datablock->top, buf);
        res = ydb_update(datablock, src);
        if (res == YDB_W_UPDATED)
            src = ynode_search(datablock->top, buf);
    }
failed:
    CLEAR_BUF(buf, buflen);
    ydb_log_out();
    if (src && ynode_type(src) == YNODE_TYPE_VAL)
        return ynode_value(src);
    return NULL;
}

struct yconn_socket_head
{
    struct
    {
        unsigned int seq;
        int fd;
    } send;
    struct
    {
        unsigned int seq;
        yconn_op op;
        ymsg_type type;
        FILE *fp;
        char *buf;
        size_t len;
    } recv;
};

void yconn_socket_deinit(yconn *conn)
{
    struct yconn_socket_head *head;
    if (!conn)
        return;
    head = conn->head;
    if (head)
    {
        if (head->send.fd > 0)
            close(head->send.fd);
        if (head->recv.fp)
            fclose(head->recv.fp);
        if (head->recv.buf)
            free(head->recv.buf);
        free(head);
    }
    conn->head = NULL;
    if (conn->fd > 0)
        close(conn->fd);
    conn->fd = -1;
    UNSET_FLAG(conn->flags, STATUS_MASK);
    SET_FLAG(conn->flags, STATUS_DISCONNECT);
}

#define DEFAULT_SOCK_PORT "9999"
ydb_res yconn_socket_init(yconn *conn)
{
    int fd = -1;
    int addrlen;
    union {
        struct sockaddr_un un;
        struct sockaddr_in in;
    } addr;

    char *address = conn->address;
    unsigned int flags = conn->flags;
    if (!IS_SET(flags, STATUS_DISCONNECT))
        return YDB_OK;
    UNSET_FLAG(flags, STATUS_MASK);
    if (IS_SET(flags, YCONN_TYPE_INET))
        fd = socket(AF_INET, SOCK_STREAM, 0);
    else // if (IS_SET(flags, YCONN_TYPE_UNIX))
        fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        yconn_errno(conn, YDB_E_SYSTEM_FAILED);
        return YDB_E_SYSTEM_FAILED;
    }
    memset(&addr, 0, sizeof(addr));

    if (IS_SET(flags, YCONN_TYPE_INET))
    {
        int ret = 1;
        int opt = 1;
        char *cport;
        in_addr_t caddr;
        char cname[128];
        strcpy(cname, &(address[strlen("tcp://")]));
        cport = strtok(cname, ":");
        cport = strtok(NULL, ":");
        ret = inet_pton(AF_INET, cname, &caddr); // INADDR_ANY;
        if (ret != 1)
        {
            yconn_errno(conn, YDB_E_SYSTEM_FAILED);
            return YDB_E_SYSTEM_FAILED;
        }
        if (!cport)
            cport = DEFAULT_SOCK_PORT;
        addr.in.sin_family = AF_INET;
        addr.in.sin_addr.s_addr = caddr;
        addr.in.sin_port = htons(atoi(cport));
        addrlen = sizeof(struct sockaddr_in);

        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
        {
            yconn_errno(conn, YDB_E_SYSTEM_FAILED);
            return YDB_E_SYSTEM_FAILED;
        }
        ydb_log_debug("addr: %s, port: %s\n", cname[0] ? cname : "null", cport);
    }
    else if (strncmp(address, "uss://", strlen("uss://")) == 0)
    {
        char *sname = &(address[strlen("uss://")]);
        addr.un.sun_family = AF_UNIX;
        snprintf(addr.un.sun_path, sizeof(addr.un.sun_path), "#%s", sname);
        addr.un.sun_path[0] = 0;
        addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(sname) + 1;
    }
    else
    {
        char *sname = &(address[strlen("us://")]);
        addr.un.sun_family = AF_UNIX;
        if (access(sname, F_OK) == 0)
            unlink(sname);
        snprintf(addr.un.sun_path, sizeof(addr.un.sun_path), "%s", sname);
        addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(sname) + 1;
    }

    if (IS_SET(flags, YCONN_ROLE_PUBLISHER))
    {
        if (bind(fd, (struct sockaddr *)&addr, addrlen) < 0)
        {
            if (connect(fd, (struct sockaddr *)&addr, addrlen) == -1)
                goto disconnected;
            SET_FLAG(flags, STATUS_CLIENT);
        }
        else
        {
            if (listen(fd, YDB_CONN_MAX) < 0)
                goto disconnected;
            SET_FLAG(flags, STATUS_SERVER);
        }
    }
    else
    {
        if (connect(fd, (struct sockaddr *)&addr, addrlen) == -1)
            goto disconnected;
        SET_FLAG(flags, STATUS_CLIENT);
    }
    if (!conn->head)
    {
        struct yconn_socket_head *head;
        head = malloc(sizeof(struct yconn_socket_head));
        if (!head)
        {
            close(fd);
            yconn_error(conn, YDB_E_MEM_ALLOC);
            return YDB_E_MEM_ALLOC;
        }
        memset(head, 0x0, sizeof(struct yconn_socket_head));
        conn->head = head;
    }
    conn->fd = fd;
    conn->flags = flags;
    return YDB_OK;
disconnected:
    yconn_errno(conn, YDB_E_CONN_FAILED);
    if (fd > 0)
        close(fd);
    UNSET_FLAG(flags, STATUS_MASK);
    SET_FLAG(flags, STATUS_DISCONNECT);
    conn->flags = flags;
    return YDB_E_CONN_FAILED;
}

int yconn_socket_accept(yconn *conn, yconn *client)
{
    int cfd = -1;
    union {
        struct sockaddr_un un;
        struct sockaddr_in in;
    } addr;
    socklen_t clen;
    if (IS_SET(conn->flags, YCONN_TYPE_INET))
        clen = sizeof(addr.in);
    else
        clen = sizeof(addr.un);
    cfd = accept(conn->fd, (struct sockaddr *)&addr, &clen);
    if (cfd < 0)
    {
        yconn_errno(conn, YDB_E_CONN_FAILED);
        return -1;
    }
    if (!client->head)
    {
        struct yconn_socket_head *head;
        head = malloc(sizeof(struct yconn_socket_head));
        if (!head)
        {
            close(cfd);
            yconn_error(conn, YDB_E_MEM_ALLOC);
            return -1;
        }
        memset(head, 0x0, sizeof(struct yconn_socket_head));
        client->head = head;
    }
    client->fd = cfd;
    if (IS_SET(conn->flags, YCONN_TYPE_INET))
    {
        char buf[128];
        char caddr[128] = {0};
        const char *client_addr = inet_ntop(AF_INET, &addr.in.sin_addr, buf, clen);
        snprintf(caddr, sizeof(caddr), "tcp://%s:%d", client_addr ? client_addr : "unknown", ntohs(addr.in.sin_port));
        ydb_log_debug("accept conn: %s\n", caddr);
        if (client->address)
            yfree(client->address);
        client->address = ystrdup(caddr);
    }
    UNSET_FLAG(client->flags, STATUS_MASK);
    SET_FLAG(client->flags, STATUS_COND_CLIENT);
    SET_FLAG(client->flags, YCONN_TYPE_UNIX);
    return cfd;
}

void yconn_socket_recv_head(yconn *conn, yconn_op *op, ymsg_type *type, unsigned int *flags, char **data, size_t *datalen)
{
    int j;
    int n = 0;
    struct yconn_socket_head *head;
    char *recvdata;
    char opstr[32];
    char typestr[32];
    head = conn->head;
    recvdata = strstr(*data, "---\n");
    if (!recvdata)
        goto failed;
    recvdata += 4;
    n = sscanf(recvdata,
               "#seq: %u\n"
               "#type: %s\n"
               "#op: %s\n",
               &head->recv.seq,
               typestr,
               opstr);
    if (n != 3)
        goto failed;
    // Operation type
    for (j = YOP_SYNC; j > YOP_NONE; j--)
    {
        if (strcmp(opstr, yconn_op_str[j]) == 0)
            break;
    }
    *op = head->recv.op = j;
    // message type (request/response/publish)
    for (j = YMSG_PUBLISH; j > YMSG_NONE; j--)
    {
        if (strcmp(typestr, ymsg_str[j]) == 0)
            break;
    }
    *type = head->recv.type = j;

    if (head->recv.op == YOP_INIT)
    {
        recvdata = strstr(recvdata, "#flags:");
        if (!recvdata)
            goto failed;
        opstr[0] = 0;
        sscanf(recvdata, "#flags: %s", opstr);
        if (opstr[0])
        {
            if (opstr[0] == 'p')
                SET_FLAG(*flags, YCONN_ROLE_PUBLISHER);
            else
                UNSET_FLAG(*flags, YCONN_ROLE_PUBLISHER);
            if (opstr[1] == 'w')
                SET_FLAG(*flags, YCONN_WRITABLE);
            else
                UNSET_FLAG(*flags, YCONN_WRITABLE);
            if (opstr[2] == 'u')
                SET_FLAG(*flags, YCONN_UNSUBSCRIBE);
            else
                UNSET_FLAG(*flags, YCONN_UNSUBSCRIBE);
        }
    }
    ydb_log_info("head {seq: %u, type: %s, op: %s}\n",
                 head->recv.seq, ymsg_str[*type], yconn_op_str[*op]);
    if (*flags)
    {
        ydb_log_info("head {flags: %s%s%s}\n",
                     IS_SET(*flags, YCONN_ROLE_PUBLISHER) ? "p" : "s",
                     IS_SET(*flags, YCONN_WRITABLE) ? "w" : "_",
                     IS_SET(*flags, YCONN_UNSUBSCRIBE) ? "u" : "_");
    }
    ydb_log_info("data {%s}\n", *data ? *data : "");
    return;
failed:
    *op = head->recv.op = YOP_NONE;
    return;
}

ydb_res yconn_default_recv(
    yconn *conn, yconn_op *op, ymsg_type *type,
    unsigned int *flags, char **data, size_t *datalen,
    int *next)
{
    ydb_res res = YDB_OK;
    struct yconn_socket_head *head;
    char recvbuf[2048 + 4];
    char *start, *end;
    ssize_t len;
    ssize_t clen;
    head = conn->head;
    *data = NULL;
    *datalen = 0;
    if (IS_SET(conn->flags, STATUS_DISCONNECT))
        return YDB_E_CONN_FAILED;

    if (*next && head->recv.fp && head->recv.buf)
    {
        start = head->recv.buf;
        end = strstr(start, "...\n");
        if (end)
        {
            clen = (end + 4) - start;
            fclose(head->recv.fp);
            start = head->recv.buf;
            len = head->recv.len;
            *data = start;
            *datalen = clen;
            yconn_socket_recv_head(conn, op, type, flags, data, datalen);
            head->recv.buf = NULL;
            head->recv.len = 0;
            head->recv.fp = NULL;
            if (len - clen > 0)
            {
                head->recv.fp = open_memstream(&head->recv.buf, &head->recv.len);
                if (!head->recv.fp)
                    goto conn_failed;
                if (fwrite(start + clen, len - clen, 1, head->recv.fp) != 1)
                    goto conn_failed;
                fflush(head->recv.fp);
                start[clen] = 0;
                *next = 1;
                return YDB_OK;
            }
            else
                *next = 0;
        }
        else
            *next = 0;
    }
    if (!head->recv.fp)
    {
        head->recv.fp = open_memstream(&head->recv.buf, &head->recv.len);
        if (!head->recv.fp)
            goto conn_failed;
    }
    if (head->recv.buf && head->recv.len >= 3)
    {
        // copy the last 3 bytes to check the message end.
        recvbuf[0] = head->recv.buf[head->recv.len - 3];
        recvbuf[1] = head->recv.buf[head->recv.len - 2];
        recvbuf[2] = head->recv.buf[head->recv.len - 1];
        recvbuf[3] = 0;
        start = &recvbuf[3];
    }
    else
        start = recvbuf;
    // len = recv(conn->fd, start, 2048, MSG_DONTWAIT);
    len = read(conn->fd, start, 2048);
    if (len <= 0)
    {
        if (len == 0)
        {
            res = YDB_E_CONN_CLOSED;
            goto conn_closed;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            goto keep_data;
        if (len < 0)
            goto conn_failed;
    }
    start[len] = 0;
    end = strstr(recvbuf, "...\n");
    if (!end)
        goto keep_data;
    clen = (end + 4) - start;
    if (fwrite(start, clen, 1, head->recv.fp) != 1)
        goto conn_failed;
    fclose(head->recv.fp);
    *data = head->recv.buf;
    *datalen = head->recv.len;
    yconn_socket_recv_head(conn, op, type, flags, data, datalen);
    head->recv.buf = NULL;
    head->recv.len = 0;
    head->recv.fp = NULL;
    if (len - clen > 0)
    {
        head->recv.fp = open_memstream(&head->recv.buf, &head->recv.len);
        if (!head->recv.fp)
            goto conn_failed;
        if (fwrite(start + clen, len - clen, 1, head->recv.fp) != 1)
            goto conn_failed;
        fflush(head->recv.fp);
        *next = 1;
    }
    else
    {
        *next = 0;
    }
    return YDB_OK;
keep_data:
    if (len > 0)
    {
        if (fwrite(start, len, 1, head->recv.fp) != 1)
            goto conn_failed;
        fflush(head->recv.fp);
    }
    *next = 0;
    return YDB_OK;
conn_failed:
    UNSET_FLAG(conn->flags, STATUS_MASK);
    SET_FLAG(conn->flags, STATUS_DISCONNECT);
    res = YDB_E_CONN_FAILED;
    yconn_errno(conn, res);
conn_closed:
    if (head->recv.fp)
        fclose(head->recv.fp);
    if (head->recv.buf)
        free(head->recv.buf);
    head->recv.fp = NULL;
    head->recv.buf = NULL;
    head->recv.len = 0;
    *next = 0;
    return res;
}

ydb_res yconn_default_send(yconn *conn, yconn_op op, ymsg_type type, char *data, size_t datalen)
{
    int n, fd;
    ydb_res res;
    char msghead[128];
    struct yconn_socket_head *head;
    ydb_log_in();
    if (IS_SET(conn->flags, STATUS_DISCONNECT))
    {
        ydb_log_out();
        return YDB_E_CONN_FAILED;
    }
    head = (struct yconn_socket_head *)conn->head;
    head->send.seq++;
    n = sprintf(msghead,
                "---\n"
                "#seq: %u\n"
                "#type: %s\n"
                "#op: %s\n",
                head->send.seq,
                ymsg_str[type],
                yconn_op_str[op]);

    ydb_log_info("head {seq: %u, type: %s, op: %s}\n",
                 head->send.seq,
                 ymsg_str[type],
                 yconn_op_str[op]);
    switch (op)
    {
    case YOP_INIT:
        n += sprintf(msghead + n,
                     "#flags: %s%s%s\n",
                     IS_SET(conn->flags, YCONN_ROLE_PUBLISHER) ? "p" : "s",
                     IS_SET(conn->flags, YCONN_WRITABLE) ? "w" : "_",
                     IS_SET(conn->flags, YCONN_UNSUBSCRIBE) ? "u" : "_");
        ydb_log_info("head {flags: %s%s%s}\n",
                     IS_SET(conn->flags, YCONN_ROLE_PUBLISHER) ? "p" : "s",
                     IS_SET(conn->flags, YCONN_WRITABLE) ? "w" : "_",
                     IS_SET(conn->flags, YCONN_UNSUBSCRIBE) ? "u" : "_");
        break;
    default:
        break;
    }
    fd = conn->fd;
    if (head->send.fd > 0)
        fd = head->send.fd;
    n = write(fd, msghead, n);
    if (n < 0)
        goto conn_failed;
    if (datalen > 0)
    {
        n = write(fd, data, datalen);
        if (n < 0)
            goto conn_failed;
    }
    ydb_log_info("data {%s}\n", data ? data : "");
    n = write(fd, "...\n", 4);
    if (n < 0)
        goto conn_failed;
    ydb_log_out();
    return YDB_OK;
conn_failed:
    UNSET_FLAG(conn->flags, STATUS_MASK);
    SET_FLAG(conn->flags, STATUS_DISCONNECT);
    res = YDB_E_CONN_FAILED;
    yconn_errno(conn, res);
    ydb_log_out();
    return res;
}

ydb_res yconn_file_init(yconn *conn)
{
    char *fname;
    char *address = conn->address;
    unsigned int flags = conn->flags;
    struct yconn_socket_head *head;
    if (!IS_SET(flags, STATUS_DISCONNECT))
        return YDB_OK;
    UNSET_FLAG(flags, STATUS_MASK);
    head = conn->head;
    if (!head)
    {
        head = malloc(sizeof(struct yconn_socket_head));
        if (!head)
        {
            yconn_error(conn, YDB_E_MEM_ALLOC);
            return YDB_E_MEM_ALLOC;
        }
        memset(head, 0x0, sizeof(struct yconn_socket_head));
        conn->head = head;
    }

    if (IS_SET(flags, YCONN_TYPE_FIFO))
    {
        char buf[256];
        fname = &(address[strlen("fifo://")]);
        strcpy(buf, fname);
        char *fi = strtok(buf, ", :");
        char *fo = strtok(NULL, ", :");
        if (!fi || !fo)
        {
            yconn_error(conn, YDB_E_STREAM_FAILED);
            return YDB_E_STREAM_FAILED;
        }

        if (access(fi, F_OK) != 0)
        {
            if (mkfifo(fi, 0666))
            {
                yconn_errno(conn, YDB_E_STREAM_FAILED);
                return YDB_E_STREAM_FAILED;
            }
        }

        if (access(fo, F_OK) != 0)
        {
            if (mkfifo(fo, 0666))
            {
                yconn_errno(conn, YDB_E_STREAM_FAILED);
                return YDB_E_STREAM_FAILED;
            }
        }
        ydb_log_debug("fi=%s, fo=%s\n", fi, fo);

        conn->fd = open(fi, O_RDWR);
        head->send.fd = open(fo, O_RDWR);
        if (conn->fd < 0 || head->send.fd < 0)
        {
            if (conn->fd > 0)
                close(conn->fd);
            if (head->send.fd > 0)
                close(head->send.fd);
            goto disconnected;
        }
    }
    else
    {
        FILE *fp;
        fname = &(address[strlen("file://")]);
        if (strcmp(fname, "stdout") == 0)
        {
            if (feof(stdout))
            {
                yconn_errno(conn, YDB_E_STREAM_FAILED);
                return YDB_E_STREAM_FAILED;
            }
            fp = stdout;
        }
        else
            fp = fopen(fname, "w");
        if (!fp)
            goto disconnected;
        conn->fd = fileno(fp);
        if (conn->fd < 0)
        {
            if (fp)
                fclose(fp);
            goto disconnected;
        }
    }
    conn->flags = flags;
    return YDB_OK;
disconnected:
    yconn_errno(conn, YDB_E_CONN_FAILED);
    free(head);
    conn->head = NULL;
    UNSET_FLAG(flags, STATUS_MASK);
    SET_FLAG(flags, STATUS_DISCONNECT);
    conn->flags = flags;
    return YDB_E_CONN_FAILED;
}

void yconn_file_deinit(yconn *conn)
{
    struct yconn_socket_head *head;
    if (!conn)
        return;
    head = conn->head;
    if (head)
    {
        if (IS_SET(conn->flags, YCONN_TYPE_FIFO))
        {
            if (head->send.fd > 0)
                close(head->send.fd);
        }
        if (head->recv.fp)
            fclose(head->recv.fp);
        if (head->recv.buf)
            free(head->recv.buf);
        free(head);
    }
    conn->head = NULL;
    if (strcmp(conn->address, "file://stdout") != 0 && conn->fd > 0)
        close(conn->fd);
    conn->fd = -1;
    UNSET_FLAG(conn->flags, STATUS_MASK);
    SET_FLAG(conn->flags, STATUS_DISCONNECT);
}

static void yconn_print(yconn *conn, const char *func, int line, char *state, bool simple)
{
    int n;
    char flagstr[128];
    if (!conn || !YDB_LOGGING_INFO)
        return;
    if (!simple)
    {
        if (state)
            ydb_logger(YDB_LOG_INFO, func, line, "%s conn:\n", state);
        ydb_logger(YDB_LOG_INFO, func, line, " fd: %d\n", conn->fd);
        ydb_logger(YDB_LOG_INFO, func, line, " address: %s\n", conn->address);
        if (IS_SET(conn->flags, YCONN_ROLE_PUBLISHER))
            n = sprintf(flagstr, "PUB");
        else
            n = sprintf(flagstr, "SUB");
        n += sprintf(flagstr + n, "(%s", IS_SET(conn->flags, YCONN_WRITABLE) ? "write" : "-");
        n += sprintf(flagstr + n, "/%s", IS_SET(conn->flags, YCONN_UNSUBSCRIBE) ? "unsub" : "-");
        n += sprintf(flagstr + n, "/%s", IS_SET(conn->flags, YCONN_RECONNECT) ? "reconn" : "-");
        n += sprintf(flagstr + n, "/%s", IS_SET(conn->flags, YCONN_UNREADABLE) ? "no-read" : "-");
        n += sprintf(flagstr + n, "/%s) ", IS_SET(conn->flags, YCONN_MAJOR_CONN) ? "major" : "");
        ydb_logger(YDB_LOG_INFO, func, line, " flags: %s\n", flagstr);

        n = sprintf(flagstr, "(%s", IS_SET(conn->flags, STATUS_DISCONNECT) ? "dis-conn" : "-");
        n += sprintf(flagstr + n, "/%s", IS_SET(conn->flags, STATUS_SERVER) ? "server" : "-");
        n += sprintf(flagstr + n, "/%s", IS_SET(conn->flags, STATUS_CLIENT) ? "client" : "-");
        n += sprintf(flagstr + n, "/%s)", IS_SET(conn->flags, STATUS_COND_CLIENT) ? "connected" : "-");
        ydb_logger(YDB_LOG_INFO, func, line, " status: %s\n", flagstr);
        if (conn->db)
        {
            ydb_logger(YDB_LOG_INFO, func, line,
                       " ydb(epollfd): %s(%d)\n",
                       conn->db->name, conn->db->epollfd);
        }
    }
    else
    {
        ydb_logger(YDB_LOG_INFO, func, line, "conn: %s (%d)\n", conn->address, conn->fd);
    }
}

static unsigned int yconn_flags(char *address, char *flagstr)
{
    unsigned int flags = 0;
    char flagbuf[256];
    strcpy(flagbuf, flagstr);
    char *token;
    token = strtok(flagbuf, ":,.- ");
    while (token)
    {
        if (strncmp(token, "subscriber", 3) == 0) // subscriber role
        {
            UNSET_FLAG(flags, YCONN_ROLE_PUBLISHER);
        }
        else if (strncmp(token, "publisher", 3) == 0) // publisher role
        {
            SET_FLAG(flags, YCONN_ROLE_PUBLISHER);
            SET_FLAG(flags, YCONN_WRITABLE);
            SET_FLAG(flags, YCONN_RECONNECT);
        }
        else if (strncmp(token, "unsubscribe", 1) == 0) // unsubscribe mode
            SET_FLAG(flags, YCONN_UNSUBSCRIBE);
        else if (strncmp(token, "writable", 1) == 0) // writable mode
            SET_FLAG(flags, YCONN_WRITABLE);
        else if (strncmp(token, "reconnect", 1) == 0) // reconnect mode
            SET_FLAG(flags, YCONN_RECONNECT);
        else if (strncmp(token, "sync-before-read", 1) == 0) // sync-before-read mode
            SET_FLAG(flags, YCONN_SYNC);
        token = strtok(NULL, ":,.- ");
    }

    if (strncmp(address, "us://", strlen("us://")) == 0 ||
        strncmp(address, "uss://", strlen("uss://")) == 0)
    {
        SET_FLAG(flags, YCONN_TYPE_UNIX);
    }
    else if (strncmp(address, "file://", strlen("file://")) == 0)
    {
        flags = 0;
        SET_FLAG(flags, YCONN_TYPE_FILE);
        SET_FLAG(flags, YCONN_WRITABLE);
        SET_FLAG(flags, YCONN_UNREADABLE);
    }
    else if (strncmp(address, "tcp://", strlen("tcp://")) == 0)
    {
        SET_FLAG(flags, YCONN_TYPE_INET);
    }
    else if (strncmp(address, "fifo://", strlen("fifo://")) == 0)
    {
        SET_FLAG(flags, YCONN_TYPE_FIFO);
    }
    // else if (strncmp(address, "ws://", strlen("ws://")) == 0)
    // else if (strncmp(address, "wss://", strlen("wss://")) == 0)
    else
    {
        return 0;
    }
    return flags;
}

static yconn *yconn_new(char *address, unsigned int flags)
{
    yconn_func_init func_init = NULL;
    yconn_func_deinit func_deinit = NULL;
    yconn_func_recv func_recv = NULL;
    yconn_func_send func_send = NULL;
    yconn_func_accept func_accept = NULL;
    yconn *conn = NULL;

    if (IS_SET(flags, YCONN_TYPE_UNIX | YCONN_TYPE_INET))
    {
        func_init = yconn_socket_init;
        func_send = yconn_default_send;
        func_recv = yconn_default_recv;
        func_accept = yconn_socket_accept;
        func_deinit = yconn_socket_deinit;
    }
    else if (IS_SET(flags, YCONN_TYPE_FILE | YCONN_TYPE_FIFO))
    {
        func_init = yconn_file_init;
        func_send = yconn_default_send;
        func_recv = yconn_default_recv;
        func_accept = NULL;
        func_deinit = yconn_file_deinit;
    }
    else
    {
        return NULL;
    }

    SET_FLAG(flags, STATUS_DISCONNECT);
    conn = malloc(sizeof(struct _yconn));
    if (!conn)
        return NULL;
    memset(conn, 0x0, sizeof(struct _yconn));
    conn->address = ystrdup(address);
    conn->flags = flags;
    conn->fd = -1;
    conn->db = NULL;
    conn->func_init = func_init;
    conn->func_send = func_send;
    conn->func_recv = func_recv;
    conn->func_accept = func_accept;
    conn->func_deinit = func_deinit;
    if (IS_SET(flags, YCONN_MAJOR_CONN))
        ytrie_insert(yconn_pool, conn->address, strlen(conn->address), conn);
    return conn;
}

static void yconn_free(yconn *conn)
{
    if (conn)
    {
        if (IS_SET(conn->flags, YCONN_MAJOR_CONN))
            ytrie_delete(yconn_pool, conn->address, strlen(conn->address));
        YDB_ASSERT(!conn->func_deinit, YDB_E_FUNC);
        conn->func_deinit(conn);
        if (conn->fd > 0)
            close(conn->fd);
        if (conn->address)
            yfree(conn->address);
        free(conn);
    }
}

static void yconn_close(yconn *conn)
{
    ydb_log_inout();
    if (conn)
    {
        YCONN_INFO(conn, "closed");
        yconn_detach(conn);
        yconn_free(conn);
    }
}

static int yconn_accept(yconn *conn)
{
    ydb_res res;
    int client_fd;
    yconn *client;
    unsigned int conn_flags;
    ydb_log_inout();
    if (!conn)
        return -1;
    conn_flags = yconn_flags(conn->address, "s:u:");
    client = yconn_new(conn->address, conn_flags);
    if (!client)
        return -1;
    YDB_ASSERT(!conn->func_accept, YDB_E_FUNC);
    client_fd = conn->func_accept(conn, client);
    if (client_fd < 0)
        goto failed;
    res = yconn_attach(client, conn->db);
    if (res)
        goto failed;
    YCONN_INFO(client, "accepted");
    return client->fd;
failed:
    yconn_detach(client);
    yconn_free(client);
    return -1;
}

static yconn *yconn_get(char *address)
{
    return ytrie_search(yconn_pool, address, strlen(address));
}

static ydb_res yconn_open(char *addr, char *flags, ydb *datablock)
{
    ydb_res res;
    yconn *conn = NULL;
    unsigned int conn_flags = 0;
    ydb_log_inout();
    conn = yconn_get(addr);
    if (conn)
        return YDB_E_ENTRY_EXISTS;
    conn_flags = yconn_flags(addr, flags);
    if (!conn_flags)
        return YDB_E_INVALID_ARGS;
    SET_FLAG(conn_flags, YCONN_MAJOR_CONN);
    conn = yconn_new(addr, conn_flags);
    if (!conn)
        return YDB_E_MEM_ALLOC;
    YDB_ASSERT(!conn->func_init, YDB_E_FUNC);
    res = conn->func_init(conn);
    if (res)
    {
        if (IS_SET(conn->flags, YCONN_RECONNECT))
        {
            if (ylist_push_back(datablock->disconn, conn))
                return YDB_OK;
        }
        yconn_free(conn);
        return res;
    }

    res = yconn_attach(conn, datablock);
    if (res)
    {
        yconn_free(conn);
        return res;
    }
    YCONN_INFO(conn, "opened");
    yconn_init(conn);
    return res;
}

static ydb_res yconn_reopen(yconn *conn, ydb *datablock)
{
    ydb_res res;
    ydb_log_inout();
    YDB_ASSERT(!conn->func_deinit, YDB_E_FUNC);
    conn->func_deinit(conn);

    YDB_ASSERT(!conn->func_init, YDB_E_FUNC);
    res = conn->func_init(conn);
    if (res)
    {
        if (IS_SET(conn->flags, YCONN_RECONNECT))
        {
            if (ylist_push_back(datablock->disconn, conn))
                return YDB_OK;
        }
        yconn_free(conn);
        return res;
    }
    res = yconn_attach(conn, datablock);
    if (res)
    {
        yconn_free(conn);
        return res;
    }

    YCONN_INFO(conn, "reopened");
    yconn_init(conn);
    return res;
}

static ydb *yconn_detach(yconn *conn)
{
    yconn *old;
    ydb *datablock;
    struct epoll_event event;
    if (!conn || !conn->db)
        return NULL;
    datablock = conn->db;
    if (IS_SET(conn->flags, YCONN_SYNC))
        datablock->synccount--;
    conn->db = NULL;

    old = ytree_delete(datablock->conn, &conn->fd);
    YDB_ASSERT(old != conn, YDB_E_PERSISTENCY_ERR);
    if (!IS_SET(conn->flags, YCONN_UNREADABLE))
    {
        event.data.ptr = old;
        event.events = EPOLLIN;
        if (epoll_ctl(datablock->epollfd, EPOLL_CTL_DEL, conn->fd, &event))
        {
            yconn_errno(conn, YDB_E_CONN_FAILED);
            return datablock;
        }
        datablock->epollcount--;
    }
    return datablock;
}

static ydb_res yconn_attach(yconn *conn, ydb *datablock)
{
    yconn *old;
    struct epoll_event event;
    if (!conn || !datablock)
        return YDB_E_INVALID_ARGS;
    if (conn->db)
    {
        if (conn->db == datablock)
            return YDB_OK;
        yconn_detach(conn);
    }

    if (!IS_SET(conn->flags, YCONN_UNREADABLE))
    {
        event.data.ptr = conn;
        event.events = EPOLLIN;
        if (epoll_ctl(datablock->epollfd, EPOLL_CTL_ADD, conn->fd, &event))
        {
            yconn_errno(conn, YDB_E_CONN_FAILED);
            return YDB_E_CONN_FAILED;
        }
        datablock->epollcount++;
    }
    old = ytree_insert(datablock->conn, &conn->fd, conn);
    YDB_ASSERT(old, YDB_E_PERSISTENCY_ERR);
    if (IS_SET(conn->flags, YCONN_SYNC))
        datablock->synccount++;
    conn->db = datablock;
    return YDB_OK;
}

static ydb_res yconn_request(yconn *conn, yconn_op op, char *buf, size_t buflen)
{
    ydb_res res = YDB_OK;
    ydb_log_inout();
    if (!conn)
        return YDB_E_INVALID_ARGS;
    YCONN_SIMPLE_INFO(conn);
    YDB_ASSERT(!conn->func_send, YDB_E_FUNC);
    res = conn->func_send(conn, op, YMSG_REQUEST, buf, buflen);
    return res;
}

static ydb_res yconn_response(yconn *conn, yconn_op op, bool done, char *buf, size_t buflen)
{
    ydb_res res = YDB_OK;
    ydb_log_inout();
    if (!conn)
        return YDB_E_INVALID_ARGS;
    YCONN_SIMPLE_INFO(conn);
    YDB_ASSERT(!conn->func_send, YDB_E_FUNC);
    res = conn->func_send(conn, op, done ? YMSG_RESPONSE : YMSG_RESP_FAILED, buf, buflen);
    return res;
}

static ydb_res yconn_publish(yconn *src_target, ydb *datablock, yconn_op op, char *buf, size_t buflen)
{
    yconn *conn;
    ylist *publist = NULL;
    ytree_iter *iter;
    ydb_log_inout();
    if (!buf)
        return YDB_E_INVALID_ARGS;
    if (op != YOP_MERGE && op != YOP_DELETE)
        return YDB_E_INVALID_MSG;
    publist = ylist_create();
    if (!publist)
        return YDB_E_MEM_ALLOC;
    ydb_log_in();
    if (datablock)
    {
        iter = ytree_first(datablock->conn);
        for (; iter != NULL; iter = ytree_next(datablock->conn, iter))
        {
            conn = ytree_data(iter);
            if (conn == src_target)
                continue;
            else if (IS_SET(conn->flags, STATUS_SERVER | STATUS_DISCONNECT))
                continue;
            else if (IS_SET(conn->flags, STATUS_CLIENT))
            {
                if (!IS_SET(conn->flags, YCONN_WRITABLE))
                    continue;
            }
            else if (IS_SET(conn->flags, STATUS_COND_CLIENT))
            {
                if (IS_SET(conn->flags, YCONN_UNSUBSCRIBE))
                    continue;
            }
            ylist_push_back(publist, conn);
        }
    }
    else
    {
        ylist_push_back(publist, src_target);
    }
    conn = ylist_pop_front(publist);
    while (conn)
    {
        YCONN_SIMPLE_INFO(conn);
        YDB_ASSERT(!conn->func_send, YDB_E_FUNC);
        conn->func_send(conn, op, YMSG_PUBLISH, buf, buflen);
        conn = ylist_pop_front(publist);
    }
    ydb_log_out();
    ylist_destroy(publist);
    return YDB_OK;
}

static ydb_res yconn_sync(yconn *src, ydb *datablock)
{
    ydb_res res = YDB_OK;
    ytree_iter *iter;
    ytree *synclist = NULL;
    struct timeval start, end;
    int timeout;

    ydb_log_in();
    YDB_FAIL(datablock->epollfd < 0, YDB_E_SYSTEM_FAILED);
    synclist = ytree_create((ytree_cmp)yconn_cmp, NULL);
    YDB_FAIL(!synclist, YDB_E_SYSTEM_FAILED);

    gettimeofday(&start, NULL);
    iter = ytree_first(datablock->conn);
    for (; iter != NULL; iter = ytree_next(datablock->conn, iter))
    {
        yconn *conn = ytree_data(iter);
        if (conn == src)
            continue;
        if (IS_SET(conn->flags, (STATUS_DISCONNECT | STATUS_SERVER | YCONN_UNREADABLE)))
            continue;
        else if (IS_SET(conn->flags, STATUS_CLIENT))
        {
            if (!IS_SET(conn->flags, YCONN_SYNC))
                continue;
        }
        else if (IS_SET(conn->flags, STATUS_COND_CLIENT))
        {
            if (!IS_SET(conn->flags, YCONN_WRITABLE))
                continue;
        }
        YCONN_SIMPLE_INFO(conn);
        res = yconn_request(conn, YOP_SYNC, NULL, 0);
        if (!res)
        {
            if (synclist)
                ytree_insert(synclist, &conn->fd, conn);
        }
    }
    ydb_log_info("sync request num: %d\n", ytree_size(synclist));
    while (ytree_size(synclist) > 0)
    {
        int i, n;
        struct epoll_event event[YDB_CONN_MAX];
        gettimeofday(&end, NULL);
        timeout = (end.tv_sec - start.tv_sec) * 1000;
        timeout = timeout + (end.tv_usec - start.tv_usec) / 1000;
        timeout = YDB_TIMEOUT - timeout;
        if (timeout > YDB_TIMEOUT || timeout < 0)
            break;
        ydb_log_debug("epoll_wait timeout %d\n", timeout);
        n = epoll_wait(datablock->epollfd, event, YDB_CONN_MAX, timeout);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            YDB_FAIL_ERRNO(n < 0, YDB_E_SYSTEM_FAILED, errno);
        }

        for (i = 0; i < n; i++)
        {
            yconn *conn = event[i].data.ptr;
            if (IS_SET(conn->flags, STATUS_SERVER))
                yconn_accept(conn);
            else
            {
                int next = 1;
                yconn_op op = YOP_NONE;
                ymsg_type type = YMSG_NONE;
                while (next)
                {
                    res = yconn_recv(conn, &op, &type, &next);
                    if (res || (op == YOP_SYNC && (type == YMSG_RESPONSE || type == YMSG_RESP_FAILED)))
                        ytree_delete(synclist, &conn->fd);
                }
            }
        }
    }
failed:
    if (YDB_LOGGING_INFO)
    {
        gettimeofday(&end, NULL);
        timeout = (end.tv_sec - start.tv_sec) * 1000;
        timeout = timeout + (end.tv_usec - start.tv_usec) / 1000;
        ydb_log_debug("start time: %u.%u\n", start.tv_sec, start.tv_usec);
        ydb_log_debug("end time: %u.%u\n", end.tv_sec, end.tv_usec);
        ydb_log_info("sync elapsed time: %d ms\n", timeout);
    }
    ytree_destroy(synclist);
    ydb_log_out();
    return res;
}

static ydb_res yconn_init(yconn *conn)
{
    ydb_res res = YDB_OK;
    struct timeval start, end;
    struct epoll_event event[YDB_CONN_MAX];
    int i, n, timeout, done = false;
    ydb *datablock = conn->db;
    if (!IS_SET(conn->flags, STATUS_CLIENT))
        return YDB_OK;
    ydb_log_in();
    YDB_FAIL(datablock->epollfd < 0, YDB_E_SYSTEM_FAILED);
    gettimeofday(&start, NULL);

    // send
    {
        char *buf = NULL;
        size_t buflen = 0;
        if (IS_SET(conn->flags, YCONN_WRITABLE) && !ydb_empty(datablock->top))
            ydb_dumps(conn->db, &buf, &buflen);
        YCONN_SIMPLE_INFO(conn);
        res = yconn_request(conn, YOP_INIT, buf, buflen);
        CLEAR_BUF(buf, buflen);
        YDB_FAIL(res, res);
    }

    // recv
    do
    {
        gettimeofday(&end, NULL);
        timeout = (end.tv_sec - start.tv_sec) * 1000;
        timeout = timeout + (end.tv_usec - start.tv_usec) / 1000;
        timeout = YDB_TIMEOUT - timeout;
        if (timeout > YDB_TIMEOUT || timeout < 0)
            break;
        ydb_log_debug("epoll_wait timeout %d\n", timeout);
        n = epoll_wait(datablock->epollfd, event, YDB_CONN_MAX, timeout);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            YDB_FAIL_ERRNO(n < 0, YDB_E_SYSTEM_FAILED, errno);
        }

        for (i = 0; i < n; i++)
        {
            yconn *rconn = event[i].data.ptr;
            if (IS_SET(rconn->flags, STATUS_SERVER))
                yconn_accept(rconn);
            else
            {
                int next = 1;
                yconn_op op = YOP_NONE;
                ymsg_type type = YMSG_NONE;
                while (next)
                {
                    res = yconn_recv(rconn, &op, &type, &next);
                    if (rconn == conn)
                    {
                        if (res)
                            done = true;
                        if (op == YOP_INIT && (type == YMSG_RESPONSE || type == YMSG_RESP_FAILED))
                            done = true;
                        ydb_log_debug("init responsed (res=%d, op=%d, type=%d)\n", res, op, type);
                    }
                }
            }
        }
    } while (!done);
failed:
    if (YDB_LOGGING_INFO)
    {
        gettimeofday(&end, NULL);
        timeout = (end.tv_sec - start.tv_sec) * 1000;
        timeout = timeout + (end.tv_usec - start.tv_usec) / 1000;
        ydb_log_debug("start time: %u.%u\n", start.tv_sec, start.tv_usec);
        ydb_log_debug("end time: %u.%u\n", end.tv_sec, end.tv_usec);
        ydb_log_info("init elapsed time: %d ms\n", timeout);
    }
    ydb_log_out();
    return res;
}

static ydb_res yconn_merge(yconn *conn, char *buf, size_t buflen)
{
    ydb_res res;
    ynode *src = NULL;
    ydb_log_in();
    res = ynode_scanf_from_buf(buf, buflen, conn->fd, &src);
    if (res)
    {
        ynode_remove(src);
        ydb_log_out();
        return res;
    }
    if (src)
    {
        ynode *top;
        ynode_log *log = NULL;
        char *logbuf = NULL;
        size_t logbuflen = 0;
        YCONN_SIMPLE_INFO(conn);
        log = ynode_log_open(conn->db->top, NULL);
        top = ynode_merge(conn->db->top, src, log);
        ynode_log_close(log, &logbuf, &logbuflen);
        ynode_remove(src);
        if (top)
        {
            conn->db->top = top;
            yconn_publish(conn, conn->db, YOP_MERGE, logbuf, logbuflen);
        }
        else
            res = YDB_E_MERGE_FAILED;
        CLEAR_BUF(logbuf, logbuflen);
    }
    ydb_log_out();
    return res;
}

// delete ydb using the input string
static ydb_res yconn_delete(yconn *conn, char *buf, size_t buflen)
{
    ydb_res res;
    ynode *src = NULL;
    unsigned int flags;
    struct ydb_delete_data ddata;
    ydb_log_in();
    res = ynode_scanf_from_buf(buf, buflen, conn->fd, &src);
    if (res)
    {
        ynode_remove(src);
        ydb_log_out();
        return res;
    }
    if (src)
    {
        char *logbuf = NULL;
        size_t logbuflen = 0;
        ddata.log = ynode_log_open(conn->db->top, NULL);
        ddata.node = conn->db->top;
        YCONN_SIMPLE_INFO(conn);
        flags = YNODE_LEAF_FIRST | YNODE_VAL_ONLY;
        res = ynode_traverse(src, ydb_delete_sub, &ddata, flags);
        ynode_log_close(ddata.log, &logbuf, &logbuflen);
        ynode_remove(src);
        if (!res)
            yconn_publish(conn, conn->db, YOP_DELETE, logbuf, logbuflen);
        CLEAR_BUF(logbuf, logbuflen);
    }
    ydb_log_out();
    return res;
}

static ydb_res yconn_recv(yconn *conn, yconn_op *op, ymsg_type *type, int *next)
{
    ydb_res res;
    char *buf = NULL;
    size_t buflen = 0;
    unsigned int flags = 0x0;
    if (IS_SET(conn->flags, STATUS_DISCONNECT))
    {
        res = YDB_E_CONN_FAILED;
    }
    else
    {
        YDB_ASSERT(!conn->func_recv, YDB_E_FUNC);
        res = conn->func_recv(conn, op, type, &flags, &buf, &buflen, next);
    }
    if (res)
    {
        *next = 0;
        CLEAR_BUF(buf, buflen);
        YCONN_INFO(conn, "disconnected");
        if (IS_SET(conn->flags, YCONN_MAJOR_CONN) &&
            IS_SET(conn->flags, YCONN_RECONNECT))
        {
            ydb *datablock = yconn_detach(conn);
            if (datablock)
            {
                if (ylist_push_back(datablock->disconn, conn))
                    return YDB_OK;
            }
        }
        yconn_detach(conn);
        yconn_free(conn);
        return YDB_OK;
    }
    switch (*type)
    {
    case YMSG_PUBLISH:
        switch (*op)
        {
        case YOP_MERGE:
            yconn_merge(conn, buf, buflen);
            break;
        case YOP_DELETE:
            yconn_delete(conn, buf, buflen);
            break;
        default:
            break;
        }
        break;
    case YMSG_REQUEST:
        switch (*op)
        {
        case YOP_MERGE:
            res = yconn_merge(conn, buf, buflen);
            yconn_response(conn, YOP_MERGE, res ? false : true, NULL, 0);
            break;
        case YOP_DELETE:
            res = yconn_delete(conn, buf, buflen);
            yconn_response(conn, YOP_DELETE, res ? false : true, NULL, 0);
            break;
        case YOP_SYNC:
            CLEAR_BUF(buf, buflen);
            res = yconn_sync(conn, conn->db);
            ydb_dumps(conn->db, &buf, &buflen);
            yconn_response(conn, YOP_SYNC, res ? false : true, buf, buflen);
            break;
        case YOP_INIT:
            if (IS_SET(conn->flags, STATUS_COND_CLIENT))
            {
                // updated flags
                conn->flags = flags | (conn->flags & (YCONN_TYPE_MASK | STATUS_MASK));
                res = yconn_merge(conn, buf, buflen);
                if (IS_SET(conn->flags, YCONN_UNSUBSCRIBE))
                {
                    yconn_response(conn, YOP_INIT, res ? false : true, NULL, 0);
                }
                else
                {
                    CLEAR_BUF(buf, buflen);
                    ydb_dumps(conn->db, &buf, &buflen);
                    yconn_response(conn, YOP_INIT, res ? false : true, buf, buflen);
                }
            }
            break;
        default:
            break;
        }
        break;
    case YMSG_RESPONSE:
        if (*op == YOP_SYNC || *op == YOP_INIT)
            yconn_merge(conn, buf, buflen);
        break;
    case YMSG_RESP_FAILED:
        break;
    default:
        break;
    }
    CLEAR_BUF(buf, buflen);
    return YDB_OK;
}

ydb_res ydb_serve(ydb *datablock, int timeout)
{
    ydb_res res;
    int i, n;
    struct epoll_event event[YDB_CONN_MAX];
    ydb_log_in();
    YDB_FAIL(!datablock, YDB_E_INVALID_ARGS);
    YDB_FAIL(datablock->epollfd < 0, YDB_E_NO_CONN);

    n = ylist_size(datablock->disconn);
    if (n > 0)
    {
        for (i = 0; i < n; i++)
        {
            yconn *conn = ylist_pop_front(datablock->disconn);
            if (!conn)
                break;
            yconn_reopen(conn, datablock);
        }
    }
    res = YDB_OK;
    n = ylist_size(datablock->disconn);
    if (n > 0)
    {
        if (timeout < 0 || timeout > YDB_TIMEOUT)
            timeout = YDB_TIMEOUT;
    }
    else
    {
        YDB_FAIL(datablock->epollcount <= 0, YDB_E_NO_CONN);
    }
    n = epoll_wait(datablock->epollfd, event, YDB_CONN_MAX, timeout);
    if (n < 0)
    {
        if (errno == EINTR)
            goto failed; // no error
        YDB_FAIL_ERRNO(n < 0, YDB_E_SYSTEM_FAILED, errno);
    }
    if (n > 0)
        ydb_log_debug("event (n=%d) received\n", n);
    for (i = 0; i < n; i++)
    {
        yconn *conn = event[i].data.ptr;
        if (IS_SET(conn->flags, STATUS_SERVER))
            yconn_accept(conn);
        else
        {
            int next = 1;
            yconn_op op = YOP_NONE;
            ymsg_type type = YMSG_NONE;
            while (next)
                yconn_recv(conn, &op, &type, &next);
        }
    }
failed:
    ydb_log_out();
    return res;
}

int ydb_fd(ydb *datablock)
{
    if (datablock)
        return datablock->epollfd;
    return -1;
}

ydb_res ydb_write_hook_add(ydb *datablock, char *path, ydb_write_hook func, char *flags, int num, ...)
{
    ydb_res res = YDB_OK;
    ynode *cur;
    void *user[5];
    unsigned int hook_flags = 0;

    ydb_log_in();
    YDB_FAIL(!datablock || !func || !path || num < 0, YDB_E_INVALID_ARGS);
    YDB_FAIL(num > 4 || num < 0, YDB_E_INVALID_ARGS);

    if (!datablock || !func)
        return YDB_E_INVALID_ARGS;

    if (flags)
    {
        if (strstr(flags, "leaf-first") == 0)
            SET_FLAG(hook_flags, YNODE_LEAF_FIRST);
        if (strstr(flags, "val-only") == 0)
            SET_FLAG(hook_flags, YNODE_VAL_ONLY);
    }

    if (path)
    {
        cur = ydb_search(datablock, path);
        YDB_FAIL(!cur, YDB_E_NO_ENTRY);
    }
    else
        cur = datablock->top;

    user[0] = datablock;
    num++;
    {
        int i;
        va_list ap;
        va_start(ap, num);
        ydb_log_debug("user total = %d\n", num);
        ydb_log_debug("user[0]=%p\n", user[0]);
        for (i = 1; i < num; i++)
        {
            void *p = va_arg(ap, void *);
            user[i + 1] = p;
            ydb_log_debug("user[%d]=%p\n", i, user[i]);
        }
        va_end(ap);
    }
    return yhook_register(cur, hook_flags, (yhook_func)func, num, user);
failed:
    ydb_log_out();
    return res;
}

void ydb_write_hook_delete(ydb *datablock, char *path)
{
    ynode *cur;
    if (!datablock)
        return;
    if (path)
    {
        cur = ydb_search(datablock, path);
        if (!cur)
            return;
    }
    else
    {
        cur = datablock->top;
    }
    yhook_unregister(cur);
}