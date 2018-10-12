#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <yaml.h>
// unix socket
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
// epoll
#include <sys/epoll.h>

#include "yalloc.h"
#include "ytree.h"
#include "ylist.h"
#include "ytrie.h"

#include "ydb.h"
#include "ynode.h"

#define IS_LAEF(x) ((x)->op == YNODE_TYPE_VAL)
#define SET_FLAG(flag, v) ((flag) = ((flag) | (v)))
#define UNSET_FLAG(flag, v) ((flag) = ((flag) & (~v)))
#define IS_SET(flag, v) ((flag) & (v))

char *ydb_err_str[] =
    {
        YDB_VNAME(YDB_OK),
        YDB_VNAME(YDB_ERR),
        YDB_VNAME(YDB_E_NO_ARGS),
        YDB_VNAME(YDB_E_TYPE_ERR),
        YDB_VNAME(YDB_E_INVALID_PARENT),
        YDB_VNAME(YDB_E_NO_ENTRY),
        YDB_VNAME(YDB_E_DUMP_CB),
        YDB_VNAME(YDB_E_MEM),
        YDB_VNAME(YDB_E_FULL_BUF),
        YDB_VNAME(YDB_E_PERSISTENCY_ERR),
        YDB_VNAME(YDB_E_INVALID_YAML_INPUT),
        YDB_VNAME(YDB_E_INVALID_YAML_TOP),
        YDB_VNAME(YDB_E_INVALID_YAML_KEY),
        YDB_VNAME(YDB_E_INVALID_YAML_ENTRY),
        YDB_VNAME(YDB_E_YAML_INIT),
        YDB_VNAME(YDB_E_YAML_EMPTY_TOKEN),
        YDB_VNAME(YDB_E_MERGE_FAILED),
        YDB_VNAME(YDB_E_SYSTEM_FAILED),
        YDB_VNAME(YDB_E_CONN_FAILED),
        YDB_VNAME(YDB_E_CONN_CLOSED),
        YDB_VNAME(YDB_E_CONN_DENIED),
        YDB_VNAME(YDB_E_INVALID_MSG),
};

int ydb_log_func_example(int severity, const char *func, int line, const char *format, ...)
{
    int len = -1;
    va_list args;
    switch (severity)
    {
    case YDB_LOG_DBG:
        printf("** ydb::dbg::%s:%d: ", func, line);
        break;
    case YDB_LOG_INOUT:
        printf("** ydb::inout::%s:%d: ", func, line);
        break;
    case YDB_LOG_INFO:
        printf("** ydb::info:%s:%d: ", func, line);
        break;
    case YDB_LOG_WARN:
        printf("** ydb::warn:%s:%d: ", func, line);
        break;
    case YDB_LOG_ERR:
        printf("** ydb::err:%s:%d: ", func, line);
        break;
    case YDB_LOG_CRI:
        printf("** ydb::cri:%s:%d: ", func, line);
        break;
    default:
        return 0;
    }
    va_start(args, format);
    len = vprintf(format, args);
    va_end(args);
    return len;
}

unsigned int ydb_log_severity = YDB_LOG_ERR;
ydb_log_func ydb_logger = ydb_log_func_example;
int ydb_log_register(ydb_log_func func)
{
    ydb_logger = func;
    return 0;
}

#define ydb_check_errno(state, caused_res, error)                              \
    do                                                                         \
    {                                                                          \
        if (state)                                                             \
        {                                                                      \
            if (ydb_log_severity < (YDB_LOG_ERR))                              \
                break;                                                         \
            if (error && ydb_logger)                                           \
                ydb_logger(YDB_LOG_ERR, __func__, __LINE__, "'%s': %s (%s)\n", \
                           #state, ydb_err_str[caused_res], strerror(errno));  \
            else if (ydb_logger)                                               \
                ydb_logger(YDB_LOG_ERR, __func__, __LINE__, "'%s': %s\n",      \
                           #state, ydb_err_str[caused_res]);                   \
            goto failed;                                                       \
        }                                                                      \
    } while (0)

#define ydb_check(state, caused_res) ydb_check_errno(state, caused_res, 0)

typedef struct _yconn yconn;

#define YCONN_ROLE_PUBLISHER 0x1
#define YCONN_ROLE_SUBSCRIBER 0x2

#define YCONN_PERMIT_RO 0x10
#define YCONN_PERMIT_WO 0x20
#define YCONN_PERMIT_RW 0x30
#define YCONN_UNSUBSCRIBE 0x40

#define STATUS_SERVER 0x100
#define STATUS_CLIENT 0x200
#define STATUS_COND_CLIENT 0x400 // connected client
#define STATUS_DISCONNECT 0x800
#define STATUS_MASK 0xf00

#define YCONN_TYPE_UNIXSOCK 0x10000

typedef enum
{
    OP_NONE,
    OP_INIT,
    OP_MERGE,
    OP_DELETE,
    OP_READ,
} yconn_op;

char *yconn_op_str[] = {
    "none",
    "init",
    "merge",
    "delete",
    "read"};

typedef enum
{
    MSG_NONE,
    MSG_REQUEST,
    MSG_RESPONSE,
    MSG_PUBLISH,
} yconn_msg_type;

char *yconn_msg_str[] = {
    "none",
    "request",
    "response",
    "pubish",
};

typedef ydb_res (*yconn_func_send)(yconn *conn, yconn_op op, yconn_msg_type type, char *data, size_t datalen);
typedef ydb_res (*yconn_func_recv)(yconn *conn, yconn_op *op, char **data, size_t *datalen, int *next);
typedef int (*yconn_func_accept)(yconn *conn, yconn *client); // return fd;

typedef ydb_res (*yconn_func_init)(yconn *conn);
typedef void (*yconn_func_deinit)(yconn *conn);

struct _yconn
{
    char *address;
    unsigned int flags;
    struct _ydb *db;
    int fd;

    yconn_func_init func_init;
    yconn_func_recv func_recv;
    yconn_func_send func_send;
    yconn_func_accept func_accept;
    yconn_func_deinit func_deinit;
    void *head;
};

yconn *yconn_open(char *address, char *flags);
void yconn_close(yconn *conn);
int yconn_denied(yconn *conn, unsigned int flags);
void yconn_print(yconn *conn, int add);
ydb_res yconn_attach(yconn *conn, ydb *datablock);
ydb *yconn_detach(yconn *conn);

int yconn_accept(yconn *conn);
ydb_res yconn_init(yconn *conn);
ydb_res yconn_request(yconn *dest, yconn_op op, char *buf, size_t buflen);
ydb_res yconn_response(yconn *dest, yconn_op op, char *buf, size_t buflen);
ydb_res yconn_publish(yconn *src, ydb *datablock, yconn_op op, char *buf, size_t buflen);
ydb_res yconn_recv(yconn *conn, yconn_op *op, char **buf, size_t *buflen, int *next);

void yconn_socket_deinit(yconn *conn);
ydb_res yconn_socket_init(yconn *conn);
int yconn_socket_accept(yconn *conn, yconn *client);
ydb_res yconn_socket_recv(yconn *conn, yconn_op *op, char **data, size_t *datalen, int *next);
ydb_res yconn_socket_send(yconn *conn, yconn_op op, yconn_msg_type type, char *data, size_t datalen);

#define yconn_errno(conn, res)            \
    ydb_log(YDB_LOG_ERR, "%s: %s (%s)\n", \
            (conn)->address, ydb_err_str[res], strerror(errno));

#define yconn_error(conn, res)       \
    ydb_log(YDB_LOG_ERR, "%s: %s\n", \
            (conn)->address, ydb_err_str[res]);

#define yconn_inout(conn) ydb_log(YDB_LOG_INOUT, "%s\n", (conn) ? ((conn)->address) : "null");

struct _ydb
{
    char *path;
    ynode *top;
    ytree *conn;
    ylist *disconn;
    int epollfd;
};

ytree *ydb_pool;

int yconn_cmp(int *fd1, int *fd2)
{
    if (*fd1 < *fd2)
        return -1;
    else if (*fd1 > *fd2)
        return 1;
    else
        return 0;
}

ydb_res ydb_pool_create()
{
    if (!ydb_pool)
    {
        ydb_pool = ytree_create((ytree_cmp)strcmp, NULL);
        if (!ydb_pool)
        {
            return YDB_E_MEM;
        }
    }
    return YDB_OK;
}

void ydb_pool_destroy()
{
    if (ydb_pool && ytree_size(ydb_pool) <= 0)
    {
        ytree_destroy(ydb_pool);
        ydb_pool = NULL;
    }
}

// open local ydb (yaml data block)
ydb *ydb_open(char *path, char *addr, char *flags)
{
    ydb_res res;
    yconn *conn = NULL;
    ydb *datablock = NULL;
    ydb_log_inout();
    ydb_check(!path, YDB_E_NO_ARGS);
    ydb_check(ydb_pool_create(), YDB_E_SYSTEM_FAILED);
    datablock = ytree_search(ydb_pool, path);
    if (datablock)
        return datablock;
    ydb_log_in();
    datablock = malloc(sizeof(ydb));
    ydb_check(!datablock, YDB_E_SYSTEM_FAILED);
    memset(datablock, 0x0, sizeof(ydb));
    datablock->path = ystrdup(path);
    ydb_check(!datablock->path, YDB_E_SYSTEM_FAILED);
    datablock->conn = ytree_create((ytree_cmp)yconn_cmp, NULL);
    ydb_check(!datablock->conn, YDB_E_SYSTEM_FAILED);

    datablock->top = ynode_create_path(path, NULL, NULL);
    ydb_check(!datablock->top, YDB_E_SYSTEM_FAILED);

    datablock->epollfd = -1;
    if (ytree_insert(ydb_pool, datablock->path, datablock))
        assert(!YDB_E_PERSISTENCY_ERR);
    if (flags)
    {
        char _addr[128];
        if (!addr)
        {
            snprintf(_addr, sizeof(_addr), "uss://%s", path);
            addr = _addr;
        }
        conn = yconn_open(addr, flags);
        ydb_check(!conn, YDB_E_SYSTEM_FAILED);
        res = yconn_attach(conn, datablock);
        ydb_check(res, YDB_E_SYSTEM_FAILED);
    }
    ydb_log_out();
    return datablock;
failed:
    yconn_close(conn);
    ydb_close(datablock);
    ydb_log_out();
    return NULL;
}

// close local ydb
void ydb_close(ydb *datablock)
{
    ydb_log_in();
    if (datablock)
    {
        if (datablock->conn)
            ytree_destroy_custom(datablock->conn, (user_free)yconn_close);
        ytree_delete(ydb_pool, datablock->path);
        if (datablock->top)
            ynode_delete(ynode_top(datablock->top), NULL);
        if (datablock->path)
            yfree(datablock->path);
        if (datablock->epollfd >= 0)
            close(datablock->epollfd);
        free(datablock);
    }
    ydb_pool_destroy();
    ydb_log_out();
}

// get the ydb
ydb *ydb_get(char *path)
{
    return ytree_search(ydb_pool, path);
}

// return the top ynode of ydb or the global root ynode of all ydb.
ynode *ydb_top(ydb *datablock)
{
    if (datablock)
        return ynode_top(datablock->top);
    return NULL;
}

// update ydb using the input string
ydb_res ydb_write(ydb *datablock, const char *format, ...)
{
    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;
    if (!datablock)
        return YDB_E_NO_ARGS;

    fp = open_memstream(&buf, &buflen);
    if (fp)
    {
        ynode *top;
        ynode *src = NULL;
        ynode_record *record = NULL;
        ydb_res res = YDB_OK;
        va_list args;
        va_start(args, format);
        vfprintf(fp, format, args);
        va_end(args);
        fclose(fp);
        res = ynode_scanf_from_buf(buf, buflen, &src);
        if (buf)
            free(buf);
        if (res)
        {
            ynode_remove(src);
            return res;
        }
        if (!src)
            return res;
        buf = NULL;
        buflen = 0;
        fp = open_memstream(&buf, &buflen);
        if (fp)
            record = ynode_record_new(fp, 0, NULL, 0, 1, YDB_LEVEL_MAX);
        top = ynode_merge(datablock->top, src, record);
        ynode_record_free(record);
        ynode_remove(src);
        if (fp)
            fclose(fp);
        if (top)
        {
            datablock->top = top;
            yconn_publish(NULL, datablock, OP_MERGE, buf, buflen);
        }
        else
            res = YDB_E_MERGE_FAILED;
        if (buf)
            free(buf);
        return res;
    }
    return YDB_E_MEM;
}

// delete ydb using the input string
extern ydb_res ynode_erase_sub(ynode *cur, void *addition);
ydb_res ydb_delete(ydb *datablock, const char *format, ...)
{
    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;
    if (!datablock)
        return YDB_E_NO_ARGS;
    fp = open_memstream(&buf, &buflen);
    if (fp)
    {
        ydb_res res;
        ynode *src = NULL;
        unsigned int flags;
        va_list args;
        va_start(args, format);
        vfprintf(fp, format, args);
        va_end(args);
        fclose(fp);
        res = ynode_scanf_from_buf(buf, buflen, &src);
        if (buf)
            free(buf);
        if (res)
        {
            ynode_remove(src);
            return res;
        }
        if (!src)
            return YDB_OK;
        ynode_dump(src, 0, 4);
        flags = YNODE_LEAF_FIRST | YNODE_LEAF_ONLY;
        res = ynode_traverse(src, ynode_erase_sub, datablock->top, flags);
        ynode_remove(src);
        return res;
    }
    return YDB_E_MEM;
}

struct ydb_read_data
{
    ydb *datablock;
    ylist *varlist;
    int vartotal;
    int varnum;
};

static ydb_res ydb_ynode_traverse(ynode *cur, void *addition)
{
    struct ydb_read_data *data = addition;
    char *value = ynode_value(cur);
    if (value && strncmp(value, "+", 1) == 0)
    {
        ynode *n = ynode_lookup(data->datablock->top, cur);
        if (n)
        {
            int index = atoi(value);
            void *p = ylist_data(ylist_index(data->varlist, index));
            // printf("index=%d p=%p\n", index, p);
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
                char *path = ynode_path(cur, YDB_LEVEL_MAX);
                ydb_log_debug("no data for (%s)\n", path);
                free(path);
            }
        }
    }
    return YDB_OK;
}

ydb_res ynode_scan(FILE *fp, char *buf, int buflen, ynode **n, int *queryform);

// read the date from ydb as the scanf()
int ydb_read(ydb *datablock, const char *format, ...)
{
    ydb_res res;
    struct ydb_read_data data;
    ynode *src = NULL;
    unsigned int flags;
    va_list ap;
    int ap_num = 0;
    if (!datablock)
        return -1;
    res = ynode_scan(NULL, (char *)format, strlen(format), &src, &ap_num);
    if (res)
    {
        ynode_remove(src);
        return -1;
    }
    if (ap_num <= 0)
    {
        ynode_remove(src);
        return 0;
    }
    data.varlist = ylist_create();
    data.vartotal = ap_num;
    data.varnum = 0;
    data.datablock = datablock;
    va_start(ap, format);
    ydb_log_debug("ap_num = %d\n", ap_num);
    do
    {
        void *p = va_arg(ap, void *);
        ylist_push_back(data.varlist, p);
        ydb_log_debug("p=%p\n", p);
        ap_num--;
    } while (ap_num > 0);
    va_end(ap);
    flags = YNODE_LEAF_FIRST | YNODE_LEAF_ONLY;
    res = ynode_traverse(src, ydb_ynode_traverse, &data, flags);
    ylist_destroy(data.varlist);
    if (res)
    {
        ynode_remove(src);
        return -1;
    }
    ynode_remove(src);
    return data.varnum;
}

// update the ydb using input path and value
// ydb_path_write(datablock, "/path/to/update=%d", value)
ydb_res ydb_path_write(ydb *datablock, const char *format, ...)
{
    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;
    if (!datablock)
        return YDB_E_NO_ARGS;
    fp = open_memstream(&buf, &buflen);
    if (fp)
    {
        ynode *src = NULL;
        va_list args;
        va_start(args, format);
        vfprintf(fp, format, args);
        va_end(args);
        fclose(fp);
        src = ynode_create_path(buf, datablock->top, NULL);
        if (buf)
            free(buf);
        if (src)
            return YDB_OK;
        return YDB_E_MERGE_FAILED;
    }
    return YDB_E_MEM;
}

// delete the ydb using input path
// ydb_path_delete(datablock, "/path/to/update\n")
ydb_res ydb_path_delete(ydb *datablock, const char *format, ...)
{
    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;
    if (!datablock)
        return YDB_E_NO_ARGS;
    fp = open_memstream(&buf, &buflen);
    if (fp)
    {
        ynode *target = NULL;
        va_list args;
        va_start(args, format);
        vfprintf(fp, format, args);
        va_end(args);
        fclose(fp);
        target = ynode_search(datablock->top, buf);
        if (buf)
            free(buf);
        if (target)
            ynode_delete(target, NULL);
        return YDB_OK;
    }
    return YDB_E_MEM;
}

// read the value from ydb using input path
// char *value = ydb_path_read(datablock, "/path/to/update")
char *ydb_path_read(ydb *datablock, const char *format, ...)
{
    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;
    if (!datablock)
        return NULL;
    fp = open_memstream(&buf, &buflen);
    if (fp)
    {
        ynode *src = NULL;
        va_list args;
        va_start(args, format);
        vfprintf(fp, format, args);
        va_end(args);
        fclose(fp);
        src = ynode_search(datablock->top, buf);
        if (buf)
            free(buf);
        if (src && ynode_type(src) == YNODE_TYPE_VAL)
            return ynode_value(src);
    }
    return NULL;
}

yconn *yconn_new(char *address, char *flags)
{
    int offset = 0;
    unsigned int flag = 0;
    yconn_func_init func_init;
    yconn_func_deinit func_deinit;
    yconn_func_recv func_recv;
    yconn_func_send func_send;
    yconn_func_accept func_accept;
    yconn *conn = NULL;

    if (strncmp(&flags[offset], "s", strlen("s")) == 0) // subscriber role
    {
        offset += strlen("s");
        SET_FLAG(flag, YCONN_ROLE_SUBSCRIBER);
        if (strncmp(&flags[offset], ":rw", strlen(":rw")) == 0)
        {
            offset += strlen(":rw");
            SET_FLAG(flag, YCONN_PERMIT_RW);
        }
        else if (strncmp(&flags[offset], ":ro", strlen(":ro")) == 0)
        {
            offset += strlen(":ro");
            SET_FLAG(flag, YCONN_PERMIT_RO);
        }
        else if (strncmp(&flags[offset], ":wo", strlen(":wo")) == 0)
        {
            offset += strlen(":wo");
            SET_FLAG(flag, YCONN_PERMIT_WO);
        }
        else
        {
            SET_FLAG(flag, YCONN_PERMIT_RO);
        }
    }
    else if (strncmp(flags, "p", strlen("p")) == 0) // publisher role
    {
        offset += strlen("p");
        SET_FLAG(flag, YCONN_ROLE_PUBLISHER);
        SET_FLAG(flag, YCONN_PERMIT_RW);
    }
    else
    {
        return NULL;
    }

    if (strncmp(&flags[offset], ":unsubscribe", strlen(":unsubscribe")) == 0)
    {
        offset += strlen(":unsubscribe");
        SET_FLAG(flag, YCONN_UNSUBSCRIBE);
    }

    if (strncmp(address, "us://", strlen("us://")) == 0 ||
        strncmp(address, "uss://", strlen("uss://")) == 0)
    {
        SET_FLAG(flag, YCONN_TYPE_UNIXSOCK);
        func_init = yconn_socket_init;
        func_send = yconn_socket_send;
        func_recv = yconn_socket_recv;
        func_accept = yconn_socket_accept;
        func_deinit = yconn_socket_deinit;
    }
    // else if (strncmp(address, "tcp://", sizeof("tcp://")) == 0)
    // else if (strncmp(address, "ws://", sizeof("ws://")) == 0)
    // else if (strncmp(address, "wss://", sizeof("wss://")) == 0)
    else
    {
        return NULL;
    }
    SET_FLAG(flag, STATUS_DISCONNECT);

    conn = malloc(sizeof(struct _yconn));
    if (!conn)
        return NULL;
    memset(conn, 0x0, sizeof(struct _yconn));
    conn->address = ystrdup(address);
    conn->flags = flag;
    conn->fd = -1;
    conn->db = NULL;
    conn->func_init = func_init;
    conn->func_send = func_send;
    conn->func_recv = func_recv;
    conn->func_accept = func_accept;
    conn->func_deinit = func_deinit;
    return conn;
}

yconn *yconn_open(char *address, char *flags)
{
    ydb_res res;
    yconn *conn = NULL;
    yconn_inout(conn);
    conn = yconn_new(address, flags);
    if (!conn)
        return NULL;
    assert(conn->func_init);
    res = conn->func_init(conn);
    if (res)
    {
        yconn_print(conn, 0);
        yconn_close(conn);
        return NULL;
    }
    yconn_inout(conn);
    if (!IS_SET(conn->flags, STATUS_DISCONNECT))
        yconn_init(conn);
    return conn;
}

void yconn_close(yconn *conn)
{
    yconn_inout(conn);
    if (conn)
    {
        assert(conn->func_deinit);
        conn->func_deinit(conn);
        if (conn->fd >= 0)
            close(conn->fd);
        if (conn->address)
            yfree(conn->address);
        free(conn);
    }
}

// close and clear conn->fd and recreate conn->fd to reconnect.
yconn *yconn_disconnect(yconn *conn)
{
    yconn_inout(conn);
    if (conn)
    {
        ydb_res res;
        assert(conn->func_deinit);
        conn->func_deinit(conn);
        assert(conn->func_init);
        res = conn->func_init(conn);
        if (res)
        {
            yconn_print(conn, 0);
            yconn_close(conn);
            return NULL;
        }
    }
    return conn;
}

int yconn_denied(yconn *conn, unsigned int flags)
{
    if (conn)
    {
        if (IS_SET(flags, YCONN_PERMIT_RO))
        {
            if (IS_SET(conn->flags, YCONN_PERMIT_RO))
                return 0;
        }
        else if (IS_SET(flags, YCONN_PERMIT_WO))
        {
            if (IS_SET(conn->flags, YCONN_PERMIT_WO))
                return 0;
        }
        return 1;
    }
    else
        return 0;
}

struct yconn_socket_head
{
    struct
    {
        unsigned int seq;
    } send;
    struct
    {
        unsigned int seq;
        yconn_op op;
        yconn_msg_type type;
        FILE *fp;
        char *buf;
        size_t len;
        int next;
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
        if (head->recv.fp)
            fclose(head->recv.fp);
        if (head->recv.buf)
            free(head->recv.buf);
    }
    if (conn->head)
        free(conn->head);
    conn->head = NULL;
    if (conn->fd >= 0)
        close(conn->fd);
    conn->fd = -1;
    UNSET_FLAG(conn->flags, STATUS_MASK);
    SET_FLAG(conn->flags, STATUS_DISCONNECT);
}

ydb_res yconn_socket_init(yconn *conn)
{
    char *sname;
    int sname_len;
    struct sockaddr_un addr;
    char *address = conn->address;
    unsigned int flags = conn->flags;
    if (!IS_SET(flags, STATUS_DISCONNECT))
        return YDB_OK;
    UNSET_FLAG(flags, STATUS_MASK);
    if (conn->fd < 0)
        conn->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (conn->fd < 0)
    {
        yconn_errno(conn, YDB_E_SYSTEM_FAILED);
        return YDB_E_SYSTEM_FAILED;
    }
    if (!conn->head)
    {
        struct yconn_socket_head *head;
        head = malloc(sizeof(struct yconn_socket_head));
        if (!head)
        {
            yconn_error(conn, YDB_E_MEM);
            return YDB_E_MEM;
        }
        memset(head, 0x0, sizeof(struct yconn_socket_head));
        conn->head = head;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (strncmp(address, "uss://", strlen("uss://")) == 0)
    {
        sname = &(address[strlen("uss://")]);
        snprintf(addr.sun_path, sizeof(addr.sun_path), "#%s", sname);
        addr.sun_path[0] = 0;
    }
    else
    {
        sname = &(address[strlen("us://")]);
        if (0 == access(sname, F_OK))
            unlink(sname);
        snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sname);
    }
    sname_len = offsetof(struct sockaddr_un, sun_path) + strlen(sname) + 1;

    if (IS_SET(flags, YCONN_ROLE_PUBLISHER))
    {
        if (bind(conn->fd, (struct sockaddr *)&addr, sname_len) < 0)
        {
            if (connect(conn->fd, (struct sockaddr *)&addr, sname_len) == -1)
                goto disconnected;
            SET_FLAG(flags, STATUS_CLIENT);
        }
        else
        {
            if (listen(conn->fd, YDB_CONN_MAX) < 0)
                goto disconnected;
            SET_FLAG(flags, STATUS_SERVER);
        }
    }
    else if (IS_SET(flags, YCONN_ROLE_SUBSCRIBER))
    {
        if (connect(conn->fd, (struct sockaddr *)&addr, sname_len) == -1)
            goto disconnected;
        SET_FLAG(flags, STATUS_CLIENT);
    }
    conn->flags = flags;
    return YDB_OK;
disconnected:
    ydb_log_warn("%s: connection failed.\n", conn->address);
    UNSET_FLAG(flags, STATUS_MASK);
    SET_FLAG(flags, STATUS_DISCONNECT);
    conn->flags = flags;
    return YDB_OK;
}

int yconn_socket_accept(yconn *conn, yconn *client)
{
    int cfd = -1;
    struct sockaddr_un addr;
    socklen_t clen = sizeof(addr);
    cfd = accept(conn->fd, &addr, &clen);
    if (cfd < 0)
    {
        yconn_errno(conn, YDB_E_CONN_FAILED);
        return -1;
    }
    client->fd = cfd;
    SET_FLAG(client->flags, YCONN_TYPE_UNIXSOCK);
    UNSET_FLAG(client->flags, STATUS_MASK);
    SET_FLAG(client->flags, STATUS_COND_CLIENT);
    if (!client->head)
    {
        struct yconn_socket_head *head;
        head = malloc(sizeof(struct yconn_socket_head));
        if (!head)
        {
            yconn_error(conn, YDB_E_MEM);
            return -1;
        }
        memset(head, 0x0, sizeof(struct yconn_socket_head));
        client->head = head;
    }
    return cfd;
}

void yconn_socket_recv_head(yconn *conn, yconn_op *op, char **data, size_t *datalen)
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
    if (n != 2)
        goto failed;
    // Operation type
    for (j = OP_READ; j > OP_NONE; j--)
    {
        if (strcmp(opstr, yconn_op_str[j]) == 0)
            break;
    }
    *op = head->recv.op = j;
    // message type (request/response/publish)
    for (j = MSG_PUBLISH; j > MSG_NONE; j--)
    {
        if (strcmp(typestr, yconn_msg_str[j]) == 0)
            break;
    }
    *op = head->recv.type = j;

    if (head->recv.op == OP_INIT)
    {
        recvdata = strstr(recvdata, "#flags:");
        if (!recvdata)
            goto failed;
        opstr[0] = 0;
        sscanf(recvdata, "#flags: %s", opstr);
        if (opstr[0])
        {
            if (opstr[0] == 'p')
                SET_FLAG(conn->flags, YCONN_ROLE_PUBLISHER);
            else
                SET_FLAG(conn->flags, YCONN_ROLE_SUBSCRIBER);
            if (opstr[1] == 'r')
                SET_FLAG(conn->flags, YCONN_PERMIT_RO);
            if (opstr[2] == 'w')
                SET_FLAG(conn->flags, YCONN_PERMIT_WO);
            if (opstr[3] == 'u')
                SET_FLAG(conn->flags, YCONN_UNSUBSCRIBE);
        }
    }
    ydb_log_info("HEAD: %s/%s\n", yconn_op_str[head->recv.op], yconn_msg_str[head->recv.type]);
    return;
failed:
    *op = head->recv.op = OP_NONE;
    return;
}

ydb_res yconn_socket_recv(yconn *conn, yconn_op *op, char **data, size_t *datalen, int *next)
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
            yconn_socket_recv_head(conn, op, data, datalen);
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
    len = recv(conn->fd, start, 32, MSG_DONTWAIT);
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
    yconn_socket_recv_head(conn, op, data, datalen);
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

ydb_res yconn_socket_send(yconn *conn, yconn_op op, yconn_msg_type type, char *data, size_t datalen)
{
    int n;
    ydb_res res;
    char msghead[128];
    struct yconn_socket_head *head;
    ydb_log_in();
    head = (struct yconn_socket_head *)conn->head;
    head->send.seq++;
    n = sprintf(msghead,
                "---\n"
                "#seq: %u\n"
                "#type: %s\n"
                "#op: %s\n",
                head->send.seq,
                yconn_msg_str[type],
                yconn_op_str[op]);
    if (op == OP_INIT)
    {
        n += sprintf(msghead + n,
                     "#flags: %s%s%s%s\n",
                     IS_SET(conn->flags, YCONN_ROLE_PUBLISHER) ? "p" : "s",
                     IS_SET(conn->flags, YCONN_PERMIT_RO) ? "r" : "-",
                     IS_SET(conn->flags, YCONN_PERMIT_WO) ? "w" : "-",
                     IS_SET(conn->flags, YCONN_UNSUBSCRIBE) ? "u" : "-");
    }
    n = send(conn->fd, msghead, n, 0);
    if (n < 0)
        goto conn_failed;
    if (datalen > 0)
    {
        n = send(conn->fd, data, datalen, 0);
        if (n < 0)
            goto conn_failed;
    }
    n = send(conn->fd, "\n...\n", 5, 0);
    if (n < 0)
        goto conn_failed;
    ydb_log_out();
    return YDB_OK;
conn_failed:
    res = YDB_E_CONN_FAILED;
    yconn_errno(conn, res);
    ydb_log_out();
    return res;
}

ydb_res yconn_attach(yconn *conn, ydb *datablock)
{
    int ret;
    yconn *old;
    struct epoll_event event;
    yconn_inout(conn);
    if (!conn || !datablock)
        return YDB_E_NO_ARGS;
    if (conn->db)
    {
        if (conn->db == datablock)
            return YDB_OK;
        else
            yconn_detach(conn);
    }

    if (datablock->epollfd < 0)
    {
        datablock->epollfd = epoll_create(YDB_CONN_MAX);
        if (datablock->epollfd < 0)
        {
            yconn_errno(conn, YDB_E_CONN_FAILED);
            return YDB_E_CONN_FAILED;
        }
    }

    event.data.ptr = conn;
    event.events = EPOLLIN;
    ret = epoll_ctl(datablock->epollfd, EPOLL_CTL_ADD, conn->fd, &event);
    if (ret)
    {
        yconn_errno(conn, YDB_E_CONN_FAILED);
        return YDB_E_CONN_FAILED;
    }
    ydb_log_info("attached fd %d to epoll %d\n", conn->fd, datablock->epollfd);
    old = ytree_insert(datablock->conn, &conn->fd, conn);
    assert(!old);
    conn->db = datablock;
    return YDB_OK;
}

ydb *yconn_detach(yconn *conn)
{
    yconn *old;
    ydb *datablock;
    struct epoll_event event;
    yconn_inout(conn);
    if (!conn)
        return NULL;
    if (!conn->db || !conn->db->conn)
        return NULL;
    datablock = conn->db;
    conn->db = NULL;
    old = ytree_delete(datablock->conn, &conn->fd);
    if (old)
    {
        int ret;
        int epollfd = datablock->epollfd;
        assert((old == conn) && YDB_E_PERSISTENCY_ERR);
        event.data.ptr = old;
        event.events = EPOLLIN;
        ret = epoll_ctl(epollfd, EPOLL_CTL_DEL, conn->fd, &event);
        if (ret)
        {
            yconn_errno(conn, YDB_E_CONN_FAILED);
            return NULL;
        }
        ydb_log_info("detached fd %d from epoll %d\n", conn->fd, datablock->epollfd);
    }
    if (ytree_size(datablock->conn) <= 0)
    {
        if (datablock->epollfd >= 0)
            close(datablock->epollfd);
        ydb_log_info("epoll %d deleted from\n", datablock->epollfd, datablock->path);
        datablock->epollfd = -1;
    }
    return datablock;
}

void yconn_print(yconn *conn, int add)
{
    int n;
    char flagstr[32];
    if (!conn)
        return;
    if (add)
        ydb_log_info("new conn:\n");
    else
        ydb_log_info("closed conn:\n");
    ydb_log_info(" fd: %d\n", conn->fd);
    ydb_log_info(" address: %s\n", conn->address);
    if (IS_SET(conn->flags, YCONN_ROLE_PUBLISHER))
        n = sprintf(flagstr, "PUB");
    else if (IS_SET(conn->flags, YCONN_ROLE_SUBSCRIBER))
        n = sprintf(flagstr, "SUB");
    else
        n = sprintf(flagstr, "LOCAL");
    n += sprintf(flagstr + n, "(%s", IS_SET(conn->flags, STATUS_DISCONNECT) ? "disconn" : "-");
    n += sprintf(flagstr + n, "%s", IS_SET(conn->flags, STATUS_SERVER) ? "server" : "-");
    n += sprintf(flagstr + n, "/%s", IS_SET(conn->flags, STATUS_CLIENT) ? "client" : "-");
    n += sprintf(flagstr + n, "/%s, ", IS_SET(conn->flags, STATUS_COND_CLIENT) ? "conn-client" : "-");
    n += sprintf(flagstr + n, "%s", IS_SET(conn->flags, YCONN_PERMIT_RO) ? "r" : "-");
    n += sprintf(flagstr + n, "%s", IS_SET(conn->flags, YCONN_PERMIT_WO) ? "w" : "-");
    n += sprintf(flagstr + n, "%s)", IS_SET(conn->flags, YCONN_UNSUBSCRIBE) ? "unsubscribe" : "-");
    ydb_log_info(" flags: %s\n", flagstr);
    ydb_log_info(" ydb: %s\n", (conn->db) ? (conn->db->path) : "null");
    ydb_log_info(" ydb->epollfd: %d\n", (conn->db) ? (conn->db->epollfd) : -1);
}

ydb_res yconn_init(yconn *conn)
{
    ydb_res res = YDB_OK;
    if (!conn)
        return YDB_E_NO_ARGS;
    if (IS_SET(conn->flags, STATUS_CLIENT))
    {
        ydb_log_in();
        assert(conn->func_send);
        res = conn->func_send(conn, OP_INIT, MSG_REQUEST, NULL, 0);
        ydb_log_out();
    }
    return res;
}

ydb_res yconn_request(yconn *dest, yconn_op op, char *buf, size_t buflen)
{
    if (!dest)
        return YDB_E_NO_ARGS;
    assert(dest->func_send);
    return dest->func_send(dest, op, MSG_REQUEST, buf, buflen);
}

ydb_res yconn_response(yconn *dest, yconn_op op, char *buf, size_t buflen)
{
    if (!dest)
        return YDB_E_NO_ARGS;
    assert(dest->func_send);
    return dest->func_send(dest, op, MSG_RESPONSE, buf, buflen);
}

ydb_res yconn_publish(yconn *src, ydb *datablock, yconn_op op, char *buf, size_t buflen)
{
    yconn *conn;
    ylist *publist = NULL;
    ytree_iter *iter;
    ydb_res res = YDB_OK;
    if (!buf)
        return YDB_E_NO_ARGS;
    if (op != OP_MERGE && op != OP_DELETE)
        return YDB_E_INVALID_MSG;
    publist = ylist_create();
    if (!publist)
        return YDB_E_MEM;
    ydb_log_in();
    iter = ytree_first(datablock->conn);
    for (; iter != NULL; iter = ytree_next(datablock->conn, iter))
    {
        conn = ytree_data(iter);
        if (conn == src)
            continue;
        else if (IS_SET(conn->flags, STATUS_SERVER))
            continue;
        else if (IS_SET(conn->flags, STATUS_CLIENT))
        {
            if (!IS_SET(conn->flags, YCONN_PERMIT_WO))
                continue;
        }
        else if (IS_SET(conn->flags, STATUS_COND_CLIENT))
        {
            if (IS_SET(conn->flags, YCONN_UNSUBSCRIBE))
                continue;
        }
        ylist_push_back(publist, conn);
    }
    conn = ylist_pop_front(publist);
    while (conn)
    {
        assert(conn->func_send);
        res = conn->func_send(conn, op, MSG_PUBLISH, buf, buflen);
        if (res)
        {
            yconn_error(conn, res);
            yconn_print(conn, 0);
            yconn_detach(conn);
            yconn_close(conn);
        }
        conn = ylist_pop_front(publist);
    }
    ydb_log_out();
    ylist_destroy(publist);
    return YDB_OK;
}

ydb_res yconn_recv(yconn *conn, yconn_op *op, char **buf, size_t *buflen, int *next)
{
    ydb_res res;
    assert(conn);
    *next = 0;
    if (IS_SET(conn->flags, STATUS_DISCONNECT))
    {
        assert(conn->func_init);
        res = conn->func_init(conn);
        if (res)
        {
            yconn_print(conn, 0);
            yconn_close(conn);
            return YDB_E_CONN_FAILED;
        }
        if (!IS_SET(conn->flags, STATUS_DISCONNECT))
            yconn_init(conn);
        return YDB_OK;
    }

    assert(conn->func_recv);
    res = conn->func_recv(conn, op, buf, buflen, next);
    if (res)
    {
        if (IS_SET(conn->flags, STATUS_COND_CLIENT))
        {
            yconn_print(conn, 0);
            yconn_detach(conn);
            yconn_close(conn);
            return res;
        }
        else
        {
            ydb *datablock;
            datablock = yconn_detach(conn);
            conn = yconn_disconnect(conn);
            res = yconn_attach(conn, datablock);
            yconn_print(conn, 1);
            return res;
        }
    }

    if (*buflen > 0)
    {
        switch (*op)
        {
        case OP_MERGE:
        case OP_DELETE:
            break;
        case OP_READ:
            break;
        case OP_INIT:
            if (!IS_SET(conn->flags, STATUS_COND_CLIENT))
                return YDB_E_CONN_DENIED;
            break;
        default:
            return YDB_E_CONN_DENIED;
        }
    }
    return YDB_OK;
}

// accept the connection and return fd;
int yconn_accept(yconn *conn)
{
    int cfd = -1;
    yconn *client = NULL;
    yconn_inout(conn);
    if (!conn)
        return -1;
    client = yconn_new(conn->address, "s");
    if (!client)
        return -1;
    assert(conn->func_accept);
    cfd = conn->func_accept(conn, client);
    if (cfd >= 0)
    {
        ydb_res res;
        res = yconn_attach(client, conn->db);
        if (!res)
        {
            yconn_print(client, 1);
            return client->fd;
        }
    }
    yconn_close(client);
    return -1;
}

int ydb_serve(ydb *datablock, int timeout)
{
    int i, n, newfd = 0;
    struct epoll_event event[YDB_CONN_MAX];
    ydb_log_in();
    ydb_check(!datablock, YDB_E_NO_ARGS);
    ydb_check(datablock->epollfd < 0, YDB_E_SYSTEM_FAILED);
    n = epoll_wait(datablock->epollfd, event, YDB_CONN_MAX, timeout);
    if (n < 0)
    {
        if (errno == EINTR)
            goto successful;
        ydb_check_errno(n < 0, YDB_E_SYSTEM_FAILED, errno);
    }

    for (i = 0; i < n; i++)
    {
        yconn *conn = event[i].data.ptr;
        if (IS_SET(conn->flags, STATUS_SERVER))
        {
            newfd = yconn_accept(conn);
            if (newfd >= 0)
                goto successful;
            continue;
        }

        int next = 0;
        do
        {
            ydb_res res;
            yconn_op op = OP_NONE;
            char *data = NULL;
            size_t datalen = 0;
            res = yconn_recv(conn, &op, &data, &datalen, &next);
            if (data)
            {
                printf(">> %s", data);
                if (!res)
                {
                    switch (op)
                    {
                    case OP_MERGE:
                        // yconn_ydb_merge(conn)
                        //  - ynode_merge() and yconn_publish(conn, buf, buflen)
                        // break;
                    case OP_DELETE:
                        // yconn_ydb_delete(conn)
                        //  - ynode_delete() and yconn_publish(conn, buf, buflen)
                    case OP_READ:
                        printf(">> %s", data);
                        break;
                    case OP_INIT:
                        if (!IS_SET(conn->flags, YCONN_UNSUBSCRIBE))
                        {
                            FILE *fp;
                            char *buf = NULL;
                            size_t buflen = 0;
                            fp = open_memstream(&buf, &buflen);
                            if (fp)
                            {
                                ynode_printf_to_fp(fp, datablock->top, 1, YDB_LEVEL_MAX);
                                fclose(fp);
                            }
                            yconn_publish(conn, datablock, OP_MERGE, buf, buflen);
                            if (buf)
                                free(buf);
                        }
                        break;
                    case OP_NONE:
                    default:
                        break;
                    }
                }
                free(data);
            }
        } while (next);
    }
successful:
    ydb_log_out();
    return newfd;
failed:
    ydb_log_out();
    return -1;
}

int ydb_fd(ydb *datablock)
{
    if (datablock)
        return datablock->epollfd;
    return -1;
}

// ydb_update_hook_add(ydb, path, callback, user)
// ydb_update_hook_delete(ydb, path)
// ydb_change_hook_add(ydb, path, callback, user, flag)
// ydb_change_hook_delete(ydb, path)
//

// subscriber
// ydb_open -> send(req, init) -> recv(req, init) -> if subscribe, send(resp, merge)
//                                                -> if unsubscribe, send(resp, none)
//                             <- recv(resp, XXX)
//
// ydb_write/ydb_delete -> if W permission, send(pub, merge) -> recv(pub, merge) -> if W, ydb_update()
// ydb_read -> if R permission && unsubscribe, send (req, merge) -> recv(req, merge) -> if R permission, send(resp, merge)
//          -> else ydb_read_local()

// publisher
// ydb_open -> do noting
// ydb_write/ydb_delete -> if W permission, send(pub, merge) -> recv(pub, merge) -> if W, ydb_update()
// ydb_read -> read local
//