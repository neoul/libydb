
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
    struct _ydb *ydb;
};

struct _ydb
{
    char *path;
    ynode *node;
    // callback ctrl data
    // ytree *hookers;
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
        ydb_root = ynode_new(YNODE_TYPE_DICT, NULL);
        if (!ydb_root)
            return YDB_E_MEM;
    }

    if (!ydb_pool)
    {
        ydb_pool = ytree_create((ytree_cmp)strcmp, NULL);
        if (!ydb_pool)
        {
            ynode_free(ydb_root);
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
            ynode_free(ydb_root);
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


ynode *ydb_fscanf(FILE *fp)
{
    ydb_res res = YDB_OK;
    int level = 0;
    yaml_parser_t parser;
    yaml_token_t token; /* new variable */
    ylist *stack;
    ynode *top = NULL;
    ynode *node = NULL;
    char *key = NULL;
    char *value = NULL;
    enum YAML_NEXT_TOKEN
    {
        YAML_NEXT_NONE,
        YAML_NEXT_MAPPING_KEY_SCALAR,
        YAML_NEXT_MAPPING_VAL_SCALAR,
        YAML_NEXT_SEQUENCE_ENTRY_SCALAR,
    } next = YAML_NEXT_NONE;

    if (!fp)
    {
        ydb_log(YDB_LOG_ERR, "no input\n");
        return NULL;
    }

    /* Initialize parser */
    if (!yaml_parser_initialize(&parser))
    {
        ydb_log_err_yaml(&parser);
        yaml_parser_delete(&parser);
        return NULL;
    }

    /* Set input file */
    yaml_parser_set_input_file(&parser, fp);

    stack = ylist_create();

    /* BEGIN new code */
    do
    {
        yaml_parser_scan(&parser, &token);
        if (!token.type)
        {
            ydb_log_err_yaml(&parser);
            break;
        }
        if (YDB_LOGGING_DEBUG)
        {
            if (token.type == YAML_BLOCK_END_TOKEN ||
                token.type == YAML_FLOW_MAPPING_END_TOKEN ||
                token.type == YAML_FLOW_SEQUENCE_END_TOKEN)
                level--;
            ydb_log(YDB_LOG_DBG, "%.*s%s\n", level * 2, space, yaml_token_str[token.type]);
            if (token.type == YAML_BLOCK_SEQUENCE_START_TOKEN ||
                token.type == YAML_BLOCK_MAPPING_START_TOKEN ||
                token.type == YAML_FLOW_SEQUENCE_START_TOKEN ||
                token.type == YAML_FLOW_MAPPING_START_TOKEN)
                level++;
        }

        switch (token.type)
        {
        case YAML_KEY_TOKEN:
            node = ylist_back(stack);
            if (node && node->type != YNODE_TYPE_DICT)
            {
                res = YDB_E_INVALID_YAML_KEY;
                break;
            }
            if (key)
            {
                ydb_log(YDB_LOG_DBG, "!!empty\n");
                node = ynode_new(YNODE_TYPE_VAL, NULL);
                ynode_attach(node, ylist_back(stack), key);
                yfree(key);
                key = NULL;
            }
            next = YAML_NEXT_MAPPING_KEY_SCALAR;
            break;
        case YAML_VALUE_TOKEN:
            next = YAML_NEXT_MAPPING_VAL_SCALAR;
            break;
            /* Block delimeters */
        case YAML_BLOCK_SEQUENCE_START_TOKEN:
        case YAML_BLOCK_MAPPING_START_TOKEN:
        case YAML_FLOW_SEQUENCE_START_TOKEN:
        case YAML_FLOW_MAPPING_START_TOKEN:
        {
            int node_type;
            if (token.type == YAML_BLOCK_SEQUENCE_START_TOKEN ||
                token.type == YAML_FLOW_SEQUENCE_START_TOKEN)
                node_type = YNODE_TYPE_LIST;
            else
                node_type = YNODE_TYPE_DICT;
            if (ylist_empty(stack))
            {
                if (top)
                {
                    if (node_type != top->type)
                    {
                        res = YDB_E_INVALID_YAML_TOP;
                        break;
                    }
                    node = top;
                }
                else
                    node = ynode_new(node_type, NULL);
            }
            else
                node = ynode_new(node_type, NULL);
            if (!node)
            {
                res = YDB_E_MEM;
                break;
            }
            ynode_attach(node, ylist_back(stack), key);
            ylist_push_back(stack, node);
            yfree(key);
            key = NULL;
            ydb_log(YDB_LOG_DBG, "last stack entry=%p\n", ylist_back(stack));
            next = YAML_NEXT_NONE;
            break;
        }
        case YAML_BLOCK_ENTRY_TOKEN:
        case YAML_FLOW_ENTRY_TOKEN:
            node = ylist_back(stack);
            if (!node || (node && node->type != YNODE_TYPE_LIST))
                res = YDB_E_INVALID_YAML_ENTRY;
            next = YAML_NEXT_SEQUENCE_ENTRY_SCALAR;
            break;
        case YAML_BLOCK_END_TOKEN:
        case YAML_FLOW_MAPPING_END_TOKEN:
        case YAML_FLOW_SEQUENCE_END_TOKEN:
            if (key)
            {
                ydb_log(YDB_LOG_DBG, "** empty ynode **\n");
                node = ynode_new(YNODE_TYPE_VAL, NULL);
                ynode_attach(node, ylist_back(stack), key);
                yfree(key);
                key = NULL;
            }
            top = ylist_pop_back(stack);
            ydb_log(YDB_LOG_DBG, "last stack entry=%p\n", ylist_back(stack));
            next = YAML_NEXT_NONE;
            break;
        case YAML_SCALAR_TOKEN:
            switch (next)
            {
            case YAML_NEXT_MAPPING_KEY_SCALAR:
                value = (char *)token.data.scalar.value;
                ydb_log(YDB_LOG_DBG, "%.*s%s\n", level * 2, space, value);
                key = ystrdup(value);
                break;
            case YAML_NEXT_MAPPING_VAL_SCALAR:
            case YAML_NEXT_SEQUENCE_ENTRY_SCALAR:
            case YAML_NEXT_NONE: // only have a scalar (leaf) node
                value = (char *)token.data.scalar.value;
                ydb_log(YDB_LOG_DBG, "%.*s%s\n", level * 2, space, value);
                node = ynode_new(YNODE_TYPE_VAL, value);
                ynode_attach(node, ylist_back(stack), key);
                if (key)
                    yfree(key);
                key = NULL;
                top = node;
                break;
            default:
                res = YDB_E_INVALID_YAML_INPUT;
                break;
            }
            next = YAML_NEXT_NONE;
            break;
        case YAML_DOCUMENT_START_TOKEN:
            break;
        case YAML_DOCUMENT_END_TOKEN:
            break;
        case YAML_STREAM_START_TOKEN:
            break;
        case YAML_STREAM_END_TOKEN:
            break;
        case YAML_TAG_TOKEN:
            ydb_log(YDB_LOG_DBG, "handle=%s suffix=%s\n", token.data.tag.handle, token.data.tag.suffix);
            break;
        /* Others */
        case YAML_VERSION_DIRECTIVE_TOKEN:
        case YAML_TAG_DIRECTIVE_TOKEN:
        case YAML_ANCHOR_TOKEN:
        case YAML_ALIAS_TOKEN:
        default:
            break;
        }
        if (res)
        {
            ydb_log_res(res);
            if (!top)
                top = ylist_front(stack);
            ynode_free(top);
            top = NULL;
            break;
        }
        if (token.type != YAML_STREAM_END_TOKEN)
        {
            yaml_token_delete(&token);
        }
    } while (token.type != YAML_STREAM_END_TOKEN);
    yaml_token_delete(&token);
    /* END new code */

    /* Cleanup */
    yaml_parser_delete(&parser);
    ylist_destroy(stack);
    if (key)
        yfree(key);
    return top;
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
