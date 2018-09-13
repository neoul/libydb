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
        YDB_VNAME(YDB_E_INVALID_YAML_INPUT),
        YDB_VNAME(YDB_E_INVALID_YAML_TOP),
        YDB_VNAME(YDB_E_INVALID_YAML_KEY),
        YDB_VNAME(YDB_E_INVALID_YAML_ENTRY),
};

int ydb_log_func_example(int severity, const char *func, int line, const char *format, ...)
{
    int len = -1;
    va_list args;
    printf("%s ",
           (severity == YDB_LOG_ERR) ? "** YDB::ERR" : (severity == YDB_LOG_INFO) ? "** YDB::INFO" : (severity == YDB_LOG_DBG) ? "** YDB::DEBUG" : "** YDB::CRI");
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
    ytrie *pre_hooks;
    ytrie *post_hooks;
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
        ydb_root = ynode_create(NULL, YNODE_TYPE_DICT, NULL, NULL);
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
    datablock->node = ynode_create_path(ydb_root, path);
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

// get the ydb top
ynode *ydb_top()
{
    return ydb_root;
}

// update ydb using the input string
int ydb_write(ydb *datablock, const char *format, ...)
{
    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;
    if (!datablock)
        return 0;
    fp = open_memstream(&buf, &buflen);
    if (fp) {
        ynode *src;
        ynode *res;
        va_list args;
        va_start(args, format);
        vfprintf(fp, format, args);
        va_end(args);
        fclose(fp);
        src = ynode_sscanf(buf, buflen);
        if (buf)
            free(buf);
        // update datablock
        // pre hook
        res = ynode_merge(datablock->node, src);
        if (res)
        {
            // failed
        }
        // post hook
        // ynode_printf(src, 0, YDB_LEVEL_MAX);
        ynode_delete(src);
    }
    return buflen;
}

// ydb = ydb_top()
// ydb = ydb_open()
// ydb_close(ydb)
// ydb_connect(/path/to/connect) open communication channel - client
//   - permission requested: ro/wo/rw
// ydb_close(/path/to/resource)
// ydb_listen(/path/to/bind) - server
//   - permission: set the permssion of the client (r/w request)
//     - r(ead): true, w(rite): false // accept only read for the client
//   - change-publishing (to others): true, false
// ydb_fprintf(FILE)
// ydb_printf(stdout)
// ydb_write(fp)
// ydb_sprintf(buffer)
// ydb_fscanf(FILE)
// ydb_scanf(stdout)
// ydb_read(fp)
// ydb_sscanf(buffer)
// ydb_attach(ynode)

// callback_type = {pre,post}
// ydb_hook_add(node, callback, callback_type)
// ydb_hook_delete(node, callback, callback_type)

// ydb communication
// Operation (crud): Create, Read, Update, Delete
// Server - Client
// @META
//   id: uuid
//   operation: c/r/u/d
//   sequence: 00001-0 (sequence-subsequence, if 0, it means this seq is done.)
// meta: (control-block for communication)
