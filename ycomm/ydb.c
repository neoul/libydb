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

#define YCONN_ROLE_LOCAL 0x0
#define YCONN_ROLE_SERVER 0x1
#define YCONN_ROLE_CLIENT 0x2

#define YCONN_PERMIT_RO 0x10
#define YCONN_PERMIT_WO 0x20
#define YCONN_PERMIT_RW 0x30

#define YCONN_SUBSCRIBE 0x40
#define YCONN_CENTER 0x80
#define YCONN_TYPE_UNIXSOCK 0x100

typedef int (*yconn_msg_build)(ydb *db, char *op, void *meta, char *data, int datalen, char **msg, int *msglen);
typedef int (*yconn_msg_parse)(ydb *db, char *msg, int msglen, char **op, void **meta, char **data, int *datalen);

struct _yconn
{
    char *address;
    unsigned int flags;
    struct _ydb *db;
    int fd;
    // yconn_msg_build msg_build;
    // yconn_msg_parse msg_parse;

    struct
    {
        FILE *fp;
        char *buf;
        size_t len;
    } recv;
};

struct _ydb
{
    char *path;
    ynode *top;
    ytree *conn;
    int epollfd;
};

ynode *ydb_root;
ytree *ydb_pool;
// ytree *conn_pool;

ydb_res ydb_pool_create()
{
    if (!ydb_root)
    {
        ydb_root = ynode_create(YNODE_TYPE_DICT, NULL, NULL, NULL);
        if (!ydb_root)
            return YDB_E_MEM;
    }

    if (!ydb_pool)
    {
        ydb_pool = ytree_create((ytree_cmp)strcmp, NULL);
        if (!ydb_pool)
        {
            ynode_delete(ydb_root);
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
            ynode_delete(ydb_root);
            ydb_root = NULL;
        }
    }
}

int yconn_cmp(int *fd1, int *fd2)
{
    if (*fd1 < *fd2)
        return -1;
    else if (*fd1 > *fd2)
        return 1;
    else
        return 0;
}

// open local ydb (yaml data block)
ydb *ydb_open(char *path)
{
    ydb *datablock = NULL;
    ynode *top = NULL;
    if (ydb_pool_create())
        assert(!YDB_E_MEM);

    datablock = ytree_search(ydb_pool, path);
    if (datablock)
        return datablock;

    datablock = malloc(sizeof(ydb));
    if (!datablock)
        assert(!YDB_E_MEM);
    memset(datablock, 0x0, sizeof(ydb));

    datablock->path = ystrdup(path);
    datablock->top = ynode_create_path(path, ydb_root);
    if (!datablock->top)
        assert(!YDB_E_MEM);

    top = ytree_insert(ydb_pool, datablock->path, datablock);
    if (top)
        assert(!YDB_E_PERSISTENCY_ERR);

    datablock->conn = ytree_create((ytree_cmp)yconn_cmp, NULL);
    if (!datablock->conn)
        assert(!YDB_E_MEM);
    datablock->epollfd = -1;
    return datablock;
}

// close local ydb
void ydb_close(ydb *datablock)
{
    if (!datablock)
        return;
    if (datablock->conn)
        ytree_destroy_custom(datablock->conn, (user_free)yconn_close);
    ytree_delete(ydb_pool, datablock->path);
    if (datablock->top)
        ynode_delete(datablock->top);
    if (datablock->path)
        yfree(datablock->path);
    if (datablock->epollfd)
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
        ydb_res res;
        ynode *top;
        ynode *src = NULL;
        va_list args;
        va_start(args, format);
        vfprintf(fp, format, args);
        va_end(args);
        fclose(fp);
        res = ynode_sscanf(buf, buflen, &src);
        if (buf)
            free(buf);
        if (res)
        {
            ynode_delete(src);
            return res;
        }
        if (!src)
            return YDB_OK;
        top = ynode_merge(datablock->top, src);
        ynode_delete(src);
        if (!top)
            return YDB_E_MERGE_FAILED;
        datablock->top = top;
        return YDB_OK;
    }
    return YDB_E_MEM;
}

static ydb_res ydb_delete_sub(ynode *cur, void *addition)
{
    ydb *datablock = (void *)addition;
    ynode *n = ynode_lookup(datablock->top, cur);
    if (n)
        ynode_delete(n);
    return YDB_OK;
}

// delete ydb using the input string
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
        res = ynode_sscanf(buf, buflen, &src);
        if (buf)
            free(buf);
        if (res)
        {
            ynode_delete(src);
            return res;
        }
        if (!src)
            return YDB_OK;
        ynode_dump(src, 0, 4);
        flags = YNODE_LEAF_FIRST | YNODE_LEAF_ONLY;
        res = ynode_traverse(src, ydb_delete_sub, datablock, flags);
        ynode_delete(src);
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
    ynode *top = NULL;
    unsigned int flags;
    va_list ap;
    int ap_num = 0;
    if (!datablock)
        return -1;
    res = ynode_scan(NULL, (char *)format, strlen(format), &top, &ap_num);
    if (res)
    {
        ynode_delete(top);
        return -1;
    }
    if (ap_num <= 0)
    {
        ynode_delete(top);
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
    res = ynode_traverse(top, ydb_ynode_traverse, &data, flags);
    ylist_destroy(data.varlist);
    if (res)
    {
        ynode_delete(top);
        return -1;
    }
    ynode_delete(top);
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
        src = ynode_create_path(buf, datablock->top);
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
        ynode *src = NULL;
        va_list args;
        va_start(args, format);
        vfprintf(fp, format, args);
        va_end(args);
        fclose(fp);
        src = ynode_search(datablock->top, buf);
        if (buf)
            free(buf);
        if (src)
            ynode_delete(src);
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
    int fd;
    int flag = 0;
    int offset = 0;
    if (!flags)
        return NULL;
    if (strncmp(&flags[offset], "c", strlen("c")) == 0) // client role
    {
        offset += strlen("c");
        SET_FLAG(flag, YCONN_ROLE_CLIENT);
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
        if (strncmp(&flags[offset], ":subscribe", strlen(":subscribe")) == 0)
        {
            offset += strlen(":subscribe");
            SET_FLAG(flag, YCONN_SUBSCRIBE);
        }
    }
    else if (strncmp(flags, "s", strlen("s")) == 0) // server role
    {
        offset += strlen("s");
        SET_FLAG(flag, YCONN_ROLE_SERVER);
        SET_FLAG(flag, YCONN_PERMIT_RW);
        SET_FLAG(flag, YCONN_SUBSCRIBE);
    }
    else if (strncmp(flags, "l", strlen("l")) == 0) // server role
    {
        offset += strlen("l");
        SET_FLAG(flag, YCONN_ROLE_LOCAL);
        SET_FLAG(flag, YCONN_PERMIT_RW);
    }
    else
    {
        return NULL;
    }

    if (strncmp(address, "us://", strlen("us://")) == 0 ||
        strncmp(address, "uss://", strlen("uss://")) == 0)
    {
        char *sname;
        int sname_len;
        struct sockaddr_un addr;
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

        if (IS_SET(flag, YCONN_ROLE_SERVER))
        {
            if (bind(fd, (struct sockaddr *)&addr, sname_len) < 0)
            {
                if (connect(fd, (struct sockaddr *)&addr, sname_len) == -1)
                    goto failed;
            }
            else
            {
                if (listen(fd, YDB_CONN_MAX) < 0)
                    goto failed;
                SET_FLAG(flag, YCONN_CENTER);
            }
        }
        else if (IS_SET(flag, YCONN_ROLE_CLIENT))
        {
            if (connect(fd, (struct sockaddr *)&addr, sname_len) == -1)
                goto failed;
        }
        SET_FLAG(flag, YCONN_TYPE_UNIXSOCK);
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
        goto failed;
    memset(conn, 0x0, sizeof(struct _yconn));
    conn->address = ystrdup(address);
    conn->flags = flag;
    conn->fd = fd;
    conn->db = NULL;
    return conn;
failed:
    if (fd >= 0)
        close(fd);
    ydb_log_error("%s: %s\n", address, strerror(errno));
    return NULL;
}

yconn *yconn_accept(yconn *conn)
{
    yconn *client = NULL;
    if (IS_SET(conn->flags, YCONN_TYPE_UNIXSOCK))
    {
        struct sockaddr_un addr;
        socklen_t clen = sizeof(addr);
        int cfd = accept(conn->fd, &addr, &clen);
        if (cfd < 0)
        {
            ydb_log_error("%s: %s\n", conn->address, strerror(errno));
            return NULL;
        }
        client = malloc(sizeof(yconn));
        if (!client)
        {
            close(cfd);
            ydb_log_res(YDB_E_MEM);
            return NULL;
        }
        memset(client, 0x0, sizeof(yconn));
        client->address = ystrdup(conn->address);
        client->fd = cfd;
        client->db = NULL;
        SET_FLAG(client->flags, YCONN_ROLE_CLIENT);
        SET_FLAG(client->flags, YCONN_TYPE_UNIXSOCK);
    }
    return client;
}

void yconn_close(yconn *conn)
{
    if (!conn)
        return;
    if (conn->recv.fp)
        fclose(conn->recv.fp);
    if (conn->recv.buf)
        free(conn->recv.buf);
    if (conn->fd)
        close(conn->fd);
    if (conn->address)
        yfree(conn->address);
    free(conn);
}

ydb_res yconn_recv(yconn *conn, char **data, size_t *datalen)
{
    ydb_res res = YDB_OK;
    if (!conn->recv.fp)
    {
        conn->recv.fp = open_memstream(&conn->recv.buf, &conn->recv.len);
        if (!conn->recv.fp)
            return YDB_E_MEM;
    }

    // if the msg is not fully received, pend fp
    if (IS_SET(conn->flags, YCONN_TYPE_UNIXSOCK))
    {
        char recvbuf[2048];
        size_t len;
        len = recv(conn->fd, recvbuf, 2048, MSG_DONTWAIT);
        if (len <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                goto keep_data;
            if (len == 0)
            {
                res = YDB_E_CONN_CLOSED;
                goto conn_closed;
            }
            goto conn_failed;
        }

        int count = fwrite(recvbuf, len, 1, conn->recv.fp);
        if (count != 1)
            goto conn_failed;
        char *start, *end;
        start = strstr(recvbuf, "---\n");
        while (start)
        {
            end = strstr(start, "...\n");
            if (!end)
                goto keep_data;
            start = strstr(end, "---\n");
        }
    }

    fclose(conn->recv.fp);
    *data = conn->recv.buf;
    *datalen = conn->recv.len;
    conn->recv.fp = NULL;
    conn->recv.buf = NULL;
    conn->recv.len = 0;
    return YDB_OK;
keep_data:
    *data = NULL;
    *datalen = 0;
    return YDB_OK;
conn_failed:
    res = YDB_E_CONN_FAILED;
    ydb_log_error("%s: %s\n", conn->address, strerror(errno));
conn_closed:
    *data = NULL;
    *datalen = 0;
    fclose(conn->recv.fp);
    if (conn->recv.buf)
        free(conn->recv.buf);
    conn->recv.fp = NULL;
    conn->recv.buf = NULL;
    conn->recv.len = 0;
    return res;
}

ydb_res yconn_send(yconn *conn, char *data, size_t datalen)
{
    int ret;
    ret = send(conn->fd, "---\n", 4, 0);
    if (ret < 0)
        goto conn_failed;

    ret = send(conn->fd, data, datalen, 0);
    if (ret < 0)
        goto conn_failed;

    ret = send(conn->fd, "...\n", 4, 0);
    if (ret < 0)
        goto conn_failed;
    return YDB_OK;
conn_failed:
    ydb_log_error("%s: %s\n", conn->address, strerror(errno));
    return YDB_E_CONN_FAILED;
}

ydb_res yconn_attach(yconn *conn, ydb *datablock)
{
    int ret;
    yconn *old;
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
            ydb_log_error("%s: %s\n", conn->address, strerror(errno));
            return YDB_E_CONN_FAILED;
        }
    }
    old = ytree_search(datablock->conn, &conn->fd);
    if (old)
        yconn_detach(old);

    event.data.ptr = conn;
    event.events = EPOLLIN;
    ret = epoll_ctl(datablock->epollfd, EPOLL_CTL_ADD, conn->fd, &event);
    if (ret)
    {
        ydb_log_error("%s: %s\n", conn->address, strerror(errno));
        return YDB_E_CONN_FAILED;
    }
    ytree_insert(datablock->conn, &conn->fd, conn);
    conn->db = datablock;
    return YDB_OK;
}

ydb_res yconn_detach(yconn *conn)
{
    yconn *old;
    struct epoll_event event;
    if (!conn)
        return YDB_E_NO_ARGS;
    if (!conn->db || !conn->db->conn)
        return YDB_OK;
    old = ytree_delete(conn->db->conn, &conn->fd);
    if (old)
    {
        int epollfd = conn->db->epollfd;
        assert((old == conn) && YDB_E_PERSISTENCY_ERR);
        event.data.ptr = old;
        event.events = EPOLLIN;
        epoll_ctl(epollfd, EPOLL_CTL_DEL, conn->fd, &event);
    }
    conn->db = NULL;
    return YDB_OK;
}

void yconn_print(yconn *conn)
{
    int n;
    char flagstr[32];
    if (!conn)
        return;
    ydb_log_debug(" conn:\n");
    ydb_log_debug("  address: %s\n", conn->address);
    if (IS_SET(conn->flags, YCONN_ROLE_SERVER))
        n = sprintf(flagstr, "s");
    else if (IS_SET(conn->flags, YCONN_ROLE_CLIENT))
        n = sprintf(flagstr, "c");
    else
        n = sprintf(flagstr, "l");
    n += sprintf(flagstr + n, "%s", IS_SET(conn->flags, YCONN_PERMIT_RO) ? "r" : "-");
    n += sprintf(flagstr + n, "%s", IS_SET(conn->flags, YCONN_PERMIT_WO) ? "w" : "-");
    ydb_log_debug("  flags: %s\n", flagstr);
    ydb_log_debug("  ydb: %s\n", (conn->db) ? (conn->db->path) : "null");
    ydb_log_debug("  fd: %d\n", conn->fd);
}

int ydb_serve(ydb *datablock, int timeout)
{
    int i, n;
    ydb_res res;
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
        if (event[i].events & (EPOLLERR | EPOLLERR))
        {
            yconn_detach(conn);
            yconn_close(conn);
            continue;
        }
        if (IS_SET(conn->flags, YCONN_CENTER))
        {
            yconn *client;
            client = yconn_accept(conn);
            if (client)
            {
                res = yconn_attach(client, datablock);
                if (res)
                    yconn_close(client);
                else
                    yconn_print(client);
            }
        }
        else
        {
            char *data = NULL;
            size_t datalen = 0;
            res = yconn_recv(conn, &data, &datalen);
            if (res)
            {
                yconn_detach(conn);
                yconn_close(conn);
            }
            if (data)
            {
                printf("%s\n", data);
                free(data);
            }
        }
    }
    return YDB_OK;
}

// yconn_request()
// yconn_response()

// +META:
//  uuid: uuid
//  operation: crud
//  sequence: 00001-0 (sequence-subsequence, if 0, it means this seq is done.)
//
// fd = yconn_open(ydb, "wss://0.0.0.0:80")
// fd = yconn_open(ydb, "ws://0.0.0.0:80")
// fd = yconn_open(ydb, "tcp://0.0.0.0:80")
// fd = yconn_open(ydb, "us://ydb-path", char *flags)
//  - permission: ro/wo/rw (client)
//  - update-rule: subscribe/not-subscribe (client)
//  - role: server/client/local
// yconn_close(ydb)
// fd = yconn_fd(ydb)
// fd = yconn_fd_first(ydb)
// fd = yconn_fd_next(ydb)
// max_fd = yconn_fd_set(ydb, &fd_set)
// fd = yconn_recv(ydb, meta, timeout) // blocking i/o
// msg = yconn_msg_builder(ydb, op, meta, ynode)
// yconn_msg_parser(ydb, msg, &op, &meta, &ynode)
// yconn_message(ydb, yconn_msg_builder, yconn_msg_parser)

// op (Operation): MRD (Merge (Create/Update), Read, Delete)
// yconn_request(ydb, meta, op, ynode)
// yconn_response(ydb, meta, op, ynode)
// yconn_publish(ydb, meta, op, ynode)

// ydb_update_hook_add(ydb, path, callback, user)
// ydb_update_hook_delete(ydb, path)
// ydb_change_hook_add(ydb, path, callback, user, flag)
// ydb_change_hook_delete(ydb, path)
//
// add the publisher id to ynode.

// client
// ydb_read(ydb, format, ...)
//  - if wo, read is failed.
//  - if subscribe flag is off, read the server ydb using yconn_request()
//  - if subscribe flag is on, read the local ydb.
//
//
// ydb_write(ydb, format, ...)
//  - if wo and rw, write input yaml to the server ydb.
//  - if ro, read is failed.

// server
// ydb_write(ydb, format, ...)
//  - yconn_publish()
// ydb_read()
//  - check ydb update callback
//  - if that exist, call the registered callback

// hook
// hooker is called in some change in ydb by MRD operation.
// ydb_hook_register(ydb, path, hooker, user, flag)
// ydb_hook_unregister(ydb, path)
