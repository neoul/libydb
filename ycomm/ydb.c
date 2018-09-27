#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <yaml.h>

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

struct _conn
{
    int fd;
    unsigned int flags;
    struct _ydb *db;
};

struct _hook
{
    char *path;
#define YDB_HOOK_PRE 0x1
#define YDB_HOOK_POST 0x2
    unsigned int flags;
    void *user;
};

struct _ydb
{
    char *path;
    ynode *node;
    // struct _conn *conn;
    // ytree *remotes;
};

ynode *ydb_root;
ytree *ydb_pool;
ytree *conn_pool;

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

// open local ydb (yaml data block)
ydb *ydb_open(char *path)
{
    ydb *datablock = NULL;
    ynode *node = NULL;
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
    datablock->node = ynode_create_path(path, ydb_root);
    if (!datablock->node)
        assert(!YDB_E_MEM);

    node = ytree_insert(ydb_pool, datablock->path, datablock);
    if (node)
        assert(!YDB_E_PERSISTENCY_ERR);

    return datablock;
}

// close local ydb
void ydb_close(ydb *datablock)
{
    if (!datablock)
        return;
    ytree_delete(ydb_pool, datablock->path);
    if (datablock->node)
        ynode_delete(datablock->node);
    if (datablock->path)
        yfree(datablock->path);
    if (datablock)
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
        return datablock->node;
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
            return 0;
        top = ynode_merge(datablock->node, src);
        ynode_delete(src);
        if (!top)
            return YDB_E_MERGE_FAILED;
        datablock->node = top;
        return YDB_OK;
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

ydb_res ydb_ynode_traverse(ynode *cur, void *addition)
{
    struct ydb_read_data *data = addition;
    char *value = ynode_value(cur);
    if (value && strncmp(value, "+", 1) == 0)
    {
        ynode *n = ynode_lookup(data->datablock->node, cur);
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
    ynode *node = NULL;
    unsigned int flags;
    va_list ap;
    int ap_num = 0;
    if (!datablock)
        return -1;
    res = ynode_scan(NULL, (char *)format, strlen(format), &node, &ap_num);
    if (res)
    {
        ynode_delete(node);
        return -1;
    }
    if (ap_num <= 0)
    {
        ynode_delete(node);
        return 0;
    }
    data.varlist = ylist_create();
    data.vartotal = ap_num;
    data.varnum = 0;
    data.datablock = datablock;
    va_start(ap, format);
    ydb_log_debug("ap_num = %d\n", ap_num);
    do {
        void *p = va_arg(ap, void *);
        ylist_push_back(data.varlist, p);
        ydb_log_debug("p=%p\n", p);
        ap_num --;
    } while (ap_num > 0);
    va_end(ap);
    flags = YNODE_TRV_LEAF_FIRST | YNODE_TRV_LEAF_ONLY;
    res = ynode_traverse(node, ydb_ynode_traverse, &data, flags);
    ylist_destroy(data.varlist);
    if (res)
    {
        ynode_delete(node);
        return -1;
    }
    ynode_delete(node);
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
        src = ynode_create_path(buf, datablock->node);
        if (buf)
            free(buf);
        if (src)
            return YDB_OK;
        return YDB_E_MERGE_FAILED;
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
        src = ynode_search(datablock->node, buf);
        if (buf)
            free(buf);
        if (src && ynode_type(src) == YNODE_TYPE_VAL)
            return ynode_value(src);
    }
    return NULL;
}

// ydb_conn_open(ydb, "unixsock://ydb-path", char *permission, unsigned int flags)
// ydb_conn_open(ydb, "tcp://0.0.0.0:80")
// ydb_conn_send(ydb, msg-head, meta, ynode)
// ydb_conn_recv(ydb, msg-head, meta, ynode)
// ydb_conn_close(ydb, "unixsock://ydb-path")
// fd, read/write
// fd_set

// request -> response (CRUD)
// notification (CRUD)
// share option each info.
// +meta: (control-block for communication)



// ydb_connect(/path/to/connect) open communication channel - client
//   - permission requested: ro/wo/rw
// ydb_close(/path/to/resource)
// ydb_listen(/path/to/bind) - server
//   - permission: set the permssion of the client (r/w request)
//     - r(ead): true, w(rite): false // accept only read for the client
//   - change-publishing (to others): true, false

// ydb communication
// Operation (crud): Create, Read, Update, Delete
// Server - Client
// @META
//   id: uuid
//   operation: c/r/u/d
//   sequence: 00001-0 (sequence-subsequence, if 0, it means this seq is done.)
// meta: (control-block for communication)
