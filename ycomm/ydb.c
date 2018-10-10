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

#define IS_LAEF(x) ((x)->type == YNODE_TYPE_VAL)
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
    case YDB_LOG_INFO:
        printf("** ydb::info:%s:%d: ", func, line);
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

typedef struct _yconn yconn;

#define YCONN_ROLE_PUBLISHER 0x1
#define YCONN_ROLE_SUBSCRIBER 0x2

#define YCONN_PERMIT_RO 0x10
#define YCONN_PERMIT_WO 0x20
#define YCONN_PERMIT_RW 0x30
#define YCONN_UNSUBSCRIBE 0x40

#define YCONN_SERVER 0x100
#define YCONN_CLIENT 0x200
#define YCONN_CONNECTED_CLIENT 0x400
#define YCONN_TYPE_UNIXSOCK 0x1000

typedef enum
{
    YCONN_MSG_NONE,
    YCONN_MSG_INIT,
    YCONN_MSG_MERGE,
    YCONN_MSG_DELETE,
    YCONN_MSG_READ,
} yconn_msg;

char *yconn_msg_str[] = {
    "none",
    "init",
    "merge",
    "delete",
    "read"};

typedef ydb_res (*yconn_func_send)(yconn *conn, yconn_msg type, char *data, size_t datalen);
typedef ydb_res (*yconn_func_recv)(yconn *conn, yconn_msg *type, char **data, size_t *datalen, int *next);
typedef yconn *(*yconn_func_accept)(yconn *conn);

typedef ydb_res (*yconn_func_init)(yconn *conn);
typedef void (*yconn_func_deinit)(yconn *conn);

struct _yconn
{
    char *address;
    unsigned int flags;
    struct _ydb *db;
    ylist_iter *iter;
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
ydb_res yconn_detach(yconn *conn);

void yconn_accept(yconn *conn);
ydb_res yconn_send(yconn *conn, yconn_msg msg, char *buf, size_t buflen);
ydb_res yconn_recv(yconn *conn, yconn_msg *type, char **buf, size_t *buflen, int *next);

void yconn_socket_deinit(yconn *conn);
ydb_res yconn_socket_init(yconn *conn);
yconn *yconn_socket_accept(yconn *conn);
ydb_res yconn_socket_recv(yconn *conn, yconn_msg *type, char **data, size_t *datalen, int *next);
ydb_res yconn_socket_send(yconn *conn, yconn_msg type, char *data, size_t datalen);

#define yconn_log_errno(res)              \
    ydb_log(YDB_LOG_ERR, "%s: %s (%s)\n", \
            conn->address, ydb_err_str[res], strerror(errno));

#define yconn_log_error(res)         \
    ydb_log(YDB_LOG_ERR, "%s: %s\n", \
            conn->address, ydb_err_str[res]);

struct _ydb
{
    char *path;
    ynode *top;
    ylist *conn;
    int epollfd;
};

ynode *ydb_root;
ytree *ydb_pool;
// ytree *conn_pool;

ydb_res ydb_pool_create()
{
    if (!ydb_root)
    {
        ydb_root = ynode_create(YNODE_TYPE_DICT, NULL, NULL, NULL, NULL);
        if (!ydb_root)
            return YDB_E_MEM;
    }

    if (!ydb_pool)
    {
        ydb_pool = ytree_create((ytree_cmp)strcmp, NULL);
        if (!ydb_pool)
        {
            ynode_delete(ydb_root, NULL);
            ydb_root = NULL;
            return YDB_E_MEM;
        }
        return YDB_OK;
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

    if (ydb_root)
    {
        if (!ydb_pool || ytree_size(ydb_pool) < 0)
        {
            ynode_delete(ydb_root, NULL);
            ydb_root = NULL;
        }
    }
}

static void ydb_hook(yhook_op_type op, ynode *cur, ynode *new, void *user)
{
    printf("== %s: %s ==\n", __func__, yhook_op_str[op]);
    if (op == YHOOK_OP_CREATE || op == YHOOK_OP_REPLACE)
        ynode_dump(new, 0, 0);
    else
        ynode_dump(cur, 0, 0);
}

// open local ydb (yaml data block)
ydb *ydb_open(char *path, char *addr, char *flags)
{
    ydb_res res;
    ydb *datablock = NULL;
    ynode *top = NULL;
    if (!path)
        return NULL;
    if (ydb_pool_create())
        return NULL;
    datablock = ytree_search(ydb_pool, path);
    if (datablock)
        return datablock;
    datablock = malloc(sizeof(ydb));
    if (!datablock)
        return NULL;
    memset(datablock, 0x0, sizeof(ydb));
    datablock->path = ystrdup(path);
    if (!datablock->path)
    {
        free(datablock);
        return NULL;
    }
    datablock->conn = ylist_create();
    if (!datablock->conn)
    {
        yfree(datablock->path);
        free(datablock);
        return NULL;
    }
    datablock->top = ynode_create_path(path, ydb_root, NULL);
    if (!datablock->top)
    {
        ylist_destroy(datablock->conn);
        yfree(datablock->path);
        free(datablock);
        return NULL;
    }
    datablock->epollfd = -1;
    top = ytree_insert(ydb_pool, datablock->path, datablock);
    if (top)
        assert(!YDB_E_PERSISTENCY_ERR);

    res = yhook_register(datablock->top, YNODE_NO_FLAG, ydb_hook, datablock);
    if (res)
    {
        ydb_close(datablock);
        return NULL;
    }
    if (flags)
    {
        yconn *conn;
        char _addr[128];
        if (!addr)
        {
            snprintf(_addr, sizeof(_addr), "uss://%s", path);
            addr = _addr;
        }
        conn = yconn_open(addr, flags);
        res = yconn_attach(conn, datablock);
        if (res)
        {
            ydb_close(datablock);
            return NULL;
        }
        res = yconn_send(conn, YCONN_MSG_INIT, NULL, 0);
        if (res && res != YDB_E_CONN_DENIED)
        {
            ydb_close(datablock);
            return NULL;
        }
    }
    return datablock;
}

// close local ydb
void ydb_close(ydb *datablock)
{
    if (!datablock)
        return;
    if (datablock->conn)
        ylist_destroy_custom(datablock->conn, (user_free)yconn_close);
    ytree_delete(ydb_pool, datablock->path);
    if (datablock->top)
        ynode_delete(datablock->top, NULL);
    if (datablock->path)
        yfree(datablock->path);
    if (datablock->epollfd >= 0)
        close(datablock->epollfd);

    free(datablock);
    ydb_pool_destroy();
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
        return datablock->top;
    return ydb_root;
}

ydb_res ydb_publish(ydb *datablock, char *buf, size_t buflen)
{
    yconn *conn;
    ylist_iter *iter;
    ydb_res res = YDB_OK;
    if (!buf)
        return YDB_E_NO_ARGS;
    ydb_log_debug("-->>\n");
    iter = ylist_first(datablock->conn);
    for (; !ylist_done(iter); iter = ylist_next(iter))
    {
        conn = ylist_data(iter);
        res = yconn_send(conn, YCONN_MSG_MERGE, buf, buflen);
        if (res)
        {
            yconn_log_error(res);
            iter = ylist_prev(iter);
            if (!iter)
                ylist_first(datablock->conn);
            yconn_print(conn, 0);
            yconn_detach(conn);
            yconn_close(conn);
        }
    }
    ydb_log_debug("<--\n");
    return YDB_OK;
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
            ydb_publish(datablock, buf, buflen);
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

yconn *yconn_open(char *address, char *flags)
{
    int flag = 0;
    int offset = 0;
    ydb_res res;
    yconn_func_init func_init;
    yconn_func_deinit func_deinit;
    yconn_func_recv func_recv;
    yconn_func_send func_send;
    yconn_func_accept func_accept;

    if (!flags)
        return NULL;
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

    struct _yconn *conn = malloc(sizeof(struct _yconn));
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

    assert(conn->func_init);
    res = conn->func_init(conn);
    if (res)
    {
        yconn_close(conn);
        return NULL;
    }
    return conn;
}

void yconn_close(yconn *conn)
{
    if (!conn)
        return;
    assert(conn->func_deinit);
    conn->func_deinit(conn);
    if (conn->fd >= 0)
        close(conn->fd);
    if (conn->address)
        yfree(conn->address);
    free(conn);
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
        yconn_msg type;
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
}

ydb_res yconn_socket_init(yconn *conn)
{
    int fd;
    char *sname;
    int sname_len;
    struct sockaddr_un addr;
    char *address = conn->address;
    unsigned int flags = conn->flags;
    struct yconn_socket_head *head;

    if (!flags)
        return YDB_E_NO_ARGS;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        goto failed;
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
        if (bind(fd, (struct sockaddr *)&addr, sname_len) < 0)
        {
            if (connect(fd, (struct sockaddr *)&addr, sname_len) == -1)
                goto failed;
            SET_FLAG(flags, YCONN_CLIENT);
        }
        else
        {
            if (listen(fd, YDB_CONN_MAX) < 0)
                goto failed;
            SET_FLAG(flags, YCONN_SERVER);
        }
    }
    else if (IS_SET(flags, YCONN_ROLE_SUBSCRIBER))
    {
        if (connect(fd, (struct sockaddr *)&addr, sname_len) == -1)
            goto failed;
        SET_FLAG(flags, YCONN_CLIENT);
    }

    head = malloc(sizeof(struct yconn_socket_head));
    if (!head)
    {
        yconn_log_error(YDB_E_MEM);
        return YDB_E_MEM;
    }
    memset(head, 0x0, sizeof(struct yconn_socket_head));

    conn->head = head;
    conn->flags = flags;
    conn->fd = fd;
    return YDB_OK;
failed:
    if (fd >= 0)
        close(fd);
    yconn_log_errno(YDB_E_CONN_FAILED);
    return YDB_E_CONN_FAILED;
}

yconn *yconn_socket_accept(yconn *conn)
{
    int cfd = -1;
    yconn *client = NULL;
    struct sockaddr_un addr;
    socklen_t clen = sizeof(addr);
    struct yconn_socket_head *head;

    cfd = accept(conn->fd, &addr, &clen);
    if (cfd < 0)
    {
        yconn_log_errno(YDB_E_CONN_FAILED);
        return NULL;
    }
    client = malloc(sizeof(yconn));
    if (!client)
    {
        close(cfd);
        yconn_log_error(YDB_E_MEM);
        return NULL;
    }
    memset(client, 0x0, sizeof(yconn));
    client->address = ystrdup(conn->address);
    client->fd = cfd;
    client->db = NULL;
    client->func_init = yconn_socket_init;
    client->func_deinit = yconn_socket_deinit;
    client->func_send = yconn_socket_send;
    client->func_recv = yconn_socket_recv;
    SET_FLAG(client->flags, YCONN_ROLE_SUBSCRIBER);
    SET_FLAG(client->flags, YCONN_CONNECTED_CLIENT);
    SET_FLAG(client->flags, YCONN_TYPE_UNIXSOCK);

    head = malloc(sizeof(struct yconn_socket_head));
    if (!head)
    {
        free(client);
        close(cfd);
        yconn_log_error(YDB_E_MEM);
        return NULL;
    }
    memset(head, 0x0, sizeof(struct yconn_socket_head));
    client->head = head;
    return client;
}

void yconn_socket_recv_head(yconn *conn, yconn_msg *type, char **data, size_t *datalen)
{
    yconn_msg j;
    int n = 0;
    struct yconn_socket_head *head;
    char *recvdata;
    char buf[32];
    head = conn->head;
    recvdata = strstr(*data, "---\n");
    if (!recvdata)
        goto failed;
    recvdata += 4;
    n = sscanf(recvdata,
               "#seq: %u\n"
               "#msg: %s\n",
               &head->recv.seq,
               buf);
    if (n != 2)
        goto failed;
    for (j = YCONN_MSG_READ; j > YCONN_MSG_NONE; j--)
    {
        if (strcmp(buf, yconn_msg_str[j]) == 0)
            break;
    }
    *type = head->recv.type = j;
    if (head->recv.type == YCONN_MSG_INIT)
    {
        recvdata = strstr(recvdata, "#flags:");
        if (!recvdata)
            goto failed;
        buf[0] = 0;
        sscanf(recvdata, "#flags: %s", buf);
        if (buf[0])
        {
            if (buf[0] == 'p')
                SET_FLAG(conn->flags, YCONN_ROLE_PUBLISHER);
            else
                SET_FLAG(conn->flags, YCONN_ROLE_SUBSCRIBER);
            if (buf[1] == 'r')
                SET_FLAG(conn->flags, YCONN_PERMIT_RO);
            if (buf[2] == 'w')
                SET_FLAG(conn->flags, YCONN_PERMIT_WO);
            if (buf[3] == 'u')
                SET_FLAG(conn->flags, YCONN_UNSUBSCRIBE);
        }
    }
    return;
failed:
    *type = head->recv.type = YCONN_MSG_NONE;
    return;
}

ydb_res yconn_socket_recv(yconn *conn, yconn_msg *type, char **data, size_t *datalen, int *next)
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
            yconn_socket_recv_head(conn, type, data, datalen);
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
    yconn_socket_recv_head(conn, type, data, datalen);
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
    yconn_log_errno(res);
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

ydb_res yconn_socket_send(yconn *conn, yconn_msg type, char *data, size_t datalen)
{
    int n;
    ydb_res res;
    char msghead[128];
    struct yconn_socket_head *head;
    head = (struct yconn_socket_head *)conn->head;
    head->send.seq++;
    n = sprintf(msghead,
                "---\n"
                "#seq: %u\n"
                "#msg: %s\n",
                head->send.seq,
                yconn_msg_str[type]);
    if (type == YCONN_MSG_INIT)
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
    return YDB_OK;
conn_failed:
    res = YDB_E_CONN_FAILED;
    yconn_log_errno(res);
    return res;
}

ydb_res yconn_attach(yconn *conn, ydb *datablock)
{
    int ret;
    struct epoll_event event;
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
            yconn_log_errno(YDB_E_CONN_FAILED);
            return YDB_E_CONN_FAILED;
        }
    }

    event.data.ptr = conn;
    event.events = EPOLLIN;
    ret = epoll_ctl(datablock->epollfd, EPOLL_CTL_ADD, conn->fd, &event);
    if (ret)
    {
        yconn_log_errno(YDB_E_CONN_FAILED);
        return YDB_E_CONN_FAILED;
    }
    conn->iter = ylist_push_back(datablock->conn, conn);
    if (!conn->iter)
    {
        yconn_log_error(YDB_E_SYSTEM_FAILED);
        epoll_ctl(datablock->epollfd, EPOLL_CTL_DEL, conn->fd, &event);
        return YDB_E_CONN_FAILED;
    }
    conn->db = datablock;
    return YDB_OK;
}

ydb_res yconn_detach(yconn *conn)
{
    int epollfd;
    struct epoll_event event;
    if (!conn)
        return YDB_E_NO_ARGS;
    if (!conn->db || !conn->iter)
        return YDB_OK;
    ylist_erase(conn->iter, NULL);
    epollfd = conn->db->epollfd;
    event.data.ptr = conn;
    event.events = EPOLLIN;
    epoll_ctl(epollfd, EPOLL_CTL_DEL, conn->fd, &event);
    conn->db = NULL;
    conn->iter = NULL;
    return YDB_OK;
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
    ydb_log_info(" address: %s\n", conn->address);
    if (IS_SET(conn->flags, YCONN_ROLE_PUBLISHER))
        n = sprintf(flagstr, "p");
    else if (IS_SET(conn->flags, YCONN_ROLE_SUBSCRIBER))
        n = sprintf(flagstr, "s");
    else
        n = sprintf(flagstr, "l");
    n += sprintf(flagstr + n, "(%s", IS_SET(conn->flags, YCONN_SERVER) ? "S" : "-");
    n += sprintf(flagstr + n, "/%s", IS_SET(conn->flags, YCONN_CLIENT) ? "C" : "-");
    n += sprintf(flagstr + n, "/%s)", IS_SET(conn->flags, YCONN_CONNECTED_CLIENT) ? "C" : "-");
    n += sprintf(flagstr + n, "%s", IS_SET(conn->flags, YCONN_PERMIT_RO) ? "r" : "-");
    n += sprintf(flagstr + n, "%s", IS_SET(conn->flags, YCONN_PERMIT_WO) ? "w" : "-");
    n += sprintf(flagstr + n, "%s", IS_SET(conn->flags, YCONN_UNSUBSCRIBE) ? "unsubscribe" : "-");
    ydb_log_info(" flags: %s\n", flagstr);
    ydb_log_info(" ydb: %s\n", (conn->db) ? (conn->db->path) : "null");
    ydb_log_info(" fd: %d\n", conn->fd);
}

// message operation: MRD (Merge (Create/Update), Read, Delete)
ydb_res yconn_send(yconn *conn, yconn_msg type, char *buf, size_t buflen)
{
    assert(conn);
    assert(conn->func_send);
    switch (type)
    {
    case YCONN_MSG_MERGE:
    case YCONN_MSG_DELETE:
        // just waiting the socket client connection
        if (IS_SET(conn->flags, YCONN_SERVER))
            return YDB_OK;
        else if (IS_SET(conn->flags, YCONN_CLIENT))
        {
            // permission check if it is the client.
            if (!IS_SET(conn->flags, YCONN_PERMIT_WO))
                return YDB_OK;
        }
        else if (IS_SET(conn->flags, YCONN_CONNECTED_CLIENT))
        {
            // check unsubscription if it is the connected client to the server.
            if (IS_SET(conn->flags, YCONN_UNSUBSCRIBE))
                return YDB_OK;
        }
        break;
    case YCONN_MSG_READ:
        if (!IS_SET(conn->flags, YCONN_PERMIT_RO))
        {
            return YDB_OK;
        }
        break;
    case YCONN_MSG_INIT:
        if (!IS_SET(conn->flags, YCONN_CLIENT))
            return YDB_OK;
        break;
    default:
        return YDB_OK;
    }
    return conn->func_send(conn, type, buf, buflen);
}

ydb_res yconn_recv(yconn *conn, yconn_msg *type, char **buf, size_t *buflen, int *next)
{
    ydb_res res;
    assert(conn);
    assert(conn->func_recv);
    res = conn->func_recv(conn, type, buf, buflen, next);
    if (res)
    {
        yconn_print(conn, 0);
        yconn_detach(conn);
        yconn_close(conn);
        return res;
    }

    if (*buflen > 0)
    {
        switch (*type)
        {
        case YCONN_MSG_MERGE:
        case YCONN_MSG_DELETE:
            break;
        case YCONN_MSG_READ:
            break;
        case YCONN_MSG_INIT:
            if (!IS_SET(conn->flags, YCONN_CONNECTED_CLIENT))
                return YDB_E_CONN_DENIED;
            break;
        default:
            return YDB_E_CONN_DENIED;
        }
    }
    return YDB_OK;
}

void yconn_accept(yconn *conn)
{
    yconn *client = NULL;
    assert(conn);
    assert(conn->func_accept);
    client = conn->func_accept(conn);
    if (client)
    {
        ydb_res res;
        res = yconn_attach(client, conn->db);
        if (res)
        {
            yconn_close(client);
            client = NULL;
        }
        else
            yconn_print(client, 1);
    }
}

int ydb_serve(ydb *datablock, int timeout)
{
    int i, n;
    struct epoll_event event[YDB_CONN_MAX];
    if (!datablock)
        return YDB_E_NO_ARGS;
    if (datablock->epollfd < 0)
        return YDB_OK;
    n = epoll_wait(datablock->epollfd, event, YDB_CONN_MAX, timeout);
    if (n < 0)
    {
        ydb_log_error("%s: %s\n", datablock->path, strerror(errno));
        return YDB_E_CONN_FAILED;
    }
    for (i = 0; i < n; i++)
    {
        yconn *conn = event[i].data.ptr;
        if (IS_SET(conn->flags, YCONN_SERVER))
        {
            yconn_accept(conn);
            continue;
        }

        int next = 0;
        do
        {
            ydb_res res;
            yconn_msg type = YCONN_MSG_NONE;
            char *data = NULL;
            size_t datalen = 0;
            res = yconn_recv(conn, &type, &data, &datalen, &next);
            if (data)
            {
                if (!res)
                {
                    switch (type)
                    {
                    case YCONN_MSG_MERGE:
                        // ydb_write(datablock, "%s", data);
                        // break;
                    case YCONN_MSG_DELETE:
                    case YCONN_MSG_READ:
                        printf(">> %s", data);
                        break;
                    case YCONN_MSG_INIT:
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
                            ydb_publish(datablock, buf, buflen);
                            if (buf)
                                free(buf);
                        }
                        break;
                    case YCONN_MSG_NONE:
                    default:
                        break;
                    }
                }
                free(data);
            }
        } while (next);
    }
    return YDB_OK;
}

// fd = yconn_open(ydb, "wss://0.0.0.0:80")
// fd = yconn_open(ydb, "ws://0.0.0.0:80")
// fd = yconn_open(ydb, "tcp://0.0.0.0:80")
// fd = yconn_open(ydb, "us://ydb-path", char *flags)
// ydb_update_hook_add(ydb, path, callback, user)
// ydb_update_hook_delete(ydb, path)
// ydb_change_hook_add(ydb, path, callback, user, flag)
// ydb_change_hook_delete(ydb, path)
//
