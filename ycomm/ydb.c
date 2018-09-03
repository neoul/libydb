#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>

#include "yalloc.h"
#include "ytree.h"
#include "ylist.h"
#include "ydb.h"

// ynode flags
#define YNODE_FLAG_KEY 0x1
#define YNODE_FLAG_ITER 0x2

struct _ynode
{
    union {
        char *key;
        ylist_iter *iter;
    };
    union {
        ylist *list;
        ytree *dict;
        char *value;
        void *data;
    };
    unsigned char flags;
    unsigned char rsvd1;
    unsigned char rsvd2;
    unsigned char type;
    struct _ynode *parent;
};

#define IS_LAEF(x) ((x)->type == YNODE_TYPE_VAL)
#define SET_FLAG(flag, v) ((flag) = ((flag) | (v)))
#define UNSET_FLAG(flag, v) ((flag) = ((flag) & (~v)))
#define IS_SET(flag, v) ((flag) & (v))

static char *ystr_convert(char *str)
{
    int slen = 0;
    unsigned int spacectrl = 0;
    unsigned int space = 0;
    unsigned int ctrl = 0;
    
    for (slen = 0; str[slen]; slen++)
    {
        if (!isgraph(str[slen]))
        {
            if (isspace(str[slen]))
            {
                if(str[slen] == ' ')
                    space++;
                else
                    spacectrl++;
            }
            else
                ctrl++;
        }
    }

    if (space == 0 && ctrl ==0 && spacectrl == 0)
        return NULL;
    else
    {
        int len = 0;
        char *newstr = malloc((slen+ spacectrl + (ctrl*3) + 4));
        if(!newstr)
            return NULL;
        newstr[len] = '"';
        len++;
        for (slen = 0; str[slen]; slen++)
        {
            if (isprint(str[slen]))
            {
                newstr[len] = str[slen];
                len++;
            }
            else if(isspace(str[slen]))
            {
                int n = sprintf(newstr+len, "\\%c", 
                    (str[slen]==0x09)?'t':
                    (str[slen]==0x0A)?'n':
                    (str[slen]==0x0B)?'v':
                    (str[slen]==0x0C)?'f':
                    (str[slen]==0x0D)?'r':' ');
                if (n <= 0)
                    break;
                len = len + n;
            }
            else 
            {
                int n = sprintf(newstr+len, "\\x%02X", str[slen]);
                if (n <= 0)
                    break;
                len = len + n;
            }
        }
        newstr[len] = '"';
        len ++;
        newstr[len] = 0;
        return newstr;
    }
}

// delete ynode regardless of the detachment of the parent
void ynode_free(ynode *node)
{
    switch (node->type)
    {
    case YNODE_TYPE_VAL:
        if (node->value)
            yfree(node->value);
        break;
    case YNODE_TYPE_DICT:
        ytree_destroy_custom(node->dict, (user_free)ynode_free);
        break;
    case YNODE_TYPE_LIST:
        ylist_destroy_custom(node->list, (user_free)ynode_free);
        break;
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    free(node);
}

// create ynode
ynode *ynode_new(unsigned char type, char *value)
{
    ynode *node = malloc(sizeof(ynode));
    if (!node)
        return NULL;
    memset(node, 0x0, sizeof(ynode));
    switch (type)
    {
    case YNODE_TYPE_VAL:
        node->value = ystrdup(value);
        if (!node->value)
            goto _error;
        break;
    case YNODE_TYPE_DICT:
        node->dict = ytree_create((ytree_cmp)strcmp, (user_free)yfree);
        if (!node->dict)
            goto _error;
        break;
    case YNODE_TYPE_LIST:
        node->list = ylist_create();
        if (!node->list)
            goto _error;
        break;
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    node->type = type;
    return node;
_error:
    free(node);
    return NULL;
}

// return parent after remove the node from the parent node.
ynode *ynode_detach(ynode *node)
{
    ynode *parent;
    ynode *searched_node;
    if (!node)
        return NULL;
    if (!node->parent)
        return NULL;
    parent = node->parent;
    node->parent = NULL;
    switch (parent->type)
    {
    case YNODE_TYPE_DICT:
        assert(node->key && YDB_E_NO_ENTRY);
        searched_node = ytree_delete(parent->dict, node->key);
        UNSET_FLAG(node->flags, YNODE_FLAG_KEY);
        node->key = NULL;
        assert(searched_node && YDB_E_NO_ENTRY);
        assert(searched_node == node && YDB_E_INVALID_PARENT);
        break;
    case YNODE_TYPE_LIST:
        assert(node->iter && YDB_E_NO_ENTRY);
        ylist_erase(node->iter, NULL);
        UNSET_FLAG(node->flags, YNODE_FLAG_ITER);
        node->iter = NULL;
        break;
    case YNODE_TYPE_VAL:
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    return parent;
}

// insert the node to the parent, the key will be used for dict node.
ydb_res ynode_attach(ynode *node, ynode *parent, char *key)
{
    ynode *old;
    if (!node || !parent)
        return YDB_E_NO_ARGS;
    if (parent->type == YNODE_TYPE_VAL)
        assert(!YDB_E_INVALID_PARENT);
    if (node->parent)
        ynode_detach(node);
    switch (parent->type)
    {
    case YNODE_TYPE_DICT:
        SET_FLAG(node->flags, YNODE_FLAG_KEY);
        node->key = ystrdup(key);
        old = ytree_insert(parent->dict, node->key, node);
        if (old)
            ynode_free(old);
        break;
    case YNODE_TYPE_LIST:
        // ignore key.
        SET_FLAG(node->flags, YNODE_FLAG_ITER);
        node->iter = ylist_push_back(parent->list, node);
        break;
    case YNODE_TYPE_VAL:
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    node->parent = parent;
    return YDB_OK;
}

#define DUMP_FLAG_DEBUG 0x1
struct dump_ctrl
{
    enum
    {
        DUMP_TYPE_FP,
        DUMP_TYPE_FD,
        DUMP_TYPE_STR,
    } type;
    FILE *fp;
    int fd;
    char *buf;
    int buflen;
    int len;
    unsigned int flags;
    int level;
};

#define S10 "          "
static char *space = S10 S10 S10 S10 S10 S10 S10 S10 S10 S10;

static struct dump_ctrl *dump_ctrl_new(FILE *fp, int fd, char *buf, int buflen, int level)
{
    struct dump_ctrl *cb;
    cb = malloc(sizeof(struct dump_ctrl));
    if (!cb)
        return NULL;
    if (fp)
        cb->type = DUMP_TYPE_FP;
    else if (fd)
        cb->type = DUMP_TYPE_FD;
    else if (buf && buflen > 0)
        cb->type = DUMP_TYPE_STR;
    else
    {
        cb->type = DUMP_TYPE_FP;
        fp = stdout;
    }
    cb->fp = fp;
    cb->fd = fd;
    cb->buf = buf;
    cb->buflen = buflen;
    cb->len = 0;
    cb->flags = 0x0;
    cb->level = level;
    return cb;
}

static struct dump_ctrl *dump_ctrl_new_debug(FILE *fp, int fd, char *buf, int buflen, int level)
{
    struct dump_ctrl *cb;
    cb = dump_ctrl_new(fp, fd, buf, buflen, level);
    if (!cb)
        return NULL;
    cb->flags = DUMP_FLAG_DEBUG;
    return cb;
}

void dump_ctrl_free(struct dump_ctrl *cb)
{
    if (cb)
        free(cb);
}

int dump_ctrl_print (struct dump_ctrl *dump, const char *format, ...)
{
    va_list args;
    va_start (args, format);
    switch (dump->type)
    {
    case DUMP_TYPE_FP:
        dump->len += vfprintf(dump->fp, format, args);
        break;
    case DUMP_TYPE_FD:
        dump->len += vdprintf(dump->fd, format, args);
        break;
    case DUMP_TYPE_STR:
        dump->len += vsnprintf((dump->buf + dump->len), (dump->buflen - dump->len), format, args);
        if (dump->buflen <= dump->len)
        {
            dump->buf[dump->buflen - 1] = 0;
            return YDB_E_FULL_BUF;
        }
        break;
    default:
        assert(!YDB_E_DUMP_CB);
    }
    va_end (args);
    return YDB_OK;
}

static int dump_ctrl_debug_ynode(struct dump_ctrl *dump, ynode *node)
{
    ydb_res res;
    int indent = dump->level * 2;
    if (indent < 0)
        return YDB_OK;
        
    // print indent
    res = dump_ctrl_print(dump, "%.*s", indent, space);
    if(res)
        return res;
    
    res = dump_ctrl_print(dump, "%p ", node);
    if(res)
        return res;
    
    // print key
    if (IS_SET(node->flags, YNODE_FLAG_KEY)) {
        char *key = ystr_convert(node->key);
        res = dump_ctrl_print(dump, "{key: %s,", key?key:node->key);
        if (key)
            free(key);
    }
    else if (IS_SET(node->flags, YNODE_FLAG_ITER))
        res = dump_ctrl_print(dump, "{key: %p,", ylist_data(node->iter));
    else
        res = dump_ctrl_print(dump, "{key: none,");
    if(res)
        return res;

    // print flags
    switch (node->type)
    {
    case YNODE_TYPE_VAL:
    {
        char *value = ystr_convert(node->value);
        res = dump_ctrl_print(dump, " value: %s,", value?value:node->value);
        if (value)
            free(value);
        break;
    }
    case YNODE_TYPE_DICT:
    {
        res = dump_ctrl_print(dump, " count: %d,", ytree_size(node->dict));
        break;
    }
    case YNODE_TYPE_LIST:
    {
        res = dump_ctrl_print(dump, " empty: %s,", ylist_empty(node->list)?"yes":"no");
        break;
    }
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    if(res)
        return res;
    res = dump_ctrl_print(dump, " parent: %p}\n", node->parent);
    return res;
}

static int dump_ctrl_print_ynode(struct dump_ctrl *dump, ynode *node)
{
    ydb_res res;
    int indent = dump->level * 2;
    if (indent < 0)
        return YDB_OK;

    // print indent
    res = dump_ctrl_print(dump, "%.*s", indent, space);
    if(res)
        return res;
    
    // print key
    if (IS_SET(node->flags, YNODE_FLAG_KEY)) {
        char *key = ystr_convert(node->key);
        res = dump_ctrl_print(dump, "%s:", key?key:node->key);
        if (key)
            free(key);
    }
    else if (IS_SET(node->flags, YNODE_FLAG_ITER))
        res = dump_ctrl_print(dump, "-");
    if(res)
        return res;

    // print value
    if (node->type == YNODE_TYPE_VAL)
    {
        char *value = ystr_convert(node->value);
        res = dump_ctrl_print(dump, " %s\n", value?value:node->value);
        if (value)
            free(value);
    }
    else
    {
        res = dump_ctrl_print(dump, "\n");
    }
    return res;
}

static int dump_ctrl_dump_ynode(struct dump_ctrl *dump, ynode *node);
static int dump_ctrl_traverse_dict(void *key, void *data, void *addition)
{
    struct dump_ctrl *dump = addition;
    ynode *node = data;
    key = (void *)key;
    return dump_ctrl_dump_ynode(dump, node);
}

static int dump_ctrl_traverse_list(void *data, void *addition)
{
    struct dump_ctrl *dump = addition;
    ynode *node = data;
    return dump_ctrl_dump_ynode(dump, node);
}

static int dump_ctrl_dump_ynode(struct dump_ctrl *dump, ynode *node)
{
    ydb_res res = YDB_OK;
    if (IS_SET(dump->flags, DUMP_FLAG_DEBUG))
        res = dump_ctrl_debug_ynode(dump, node);
    else
        res = dump_ctrl_print_ynode(dump, node);
    // printf("\ndump len=%d\n", dump->len);
    if (res)
        return res;
    dump->level++;
    switch (node->type)
    {
    case YNODE_TYPE_DICT:
        res = ytree_traverse(node->dict, dump_ctrl_traverse_dict, dump);
        break;
    case YNODE_TYPE_LIST:
        res = ylist_traverse(node->list, dump_ctrl_traverse_list, dump);
        break;
    case YNODE_TYPE_VAL:
        break;
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    dump->level--;
    return res;
}

void ynode_dump_debug(ynode *node, unsigned int level)
{
    struct dump_ctrl *dump;
    if (!node)
        return;
    dump = dump_ctrl_new_debug(NULL, 0, NULL, 0, level);
    if (!dump)
        return;
    printf("\n[dump (for debug)]\n");
    dump_ctrl_dump_ynode(dump, node);
    printf("[dump (len=%d)]\n", dump->len);
    dump_ctrl_free(dump);
}

void ynode_dump(ynode *node, unsigned int level)
{
    struct dump_ctrl *dump;
    if (!node)
        return;
    dump = dump_ctrl_new(NULL, 0, NULL, 0, level);
    if (!dump)
        return;
    printf("\n[dump]\n");
    dump_ctrl_dump_ynode(dump, node);
    printf("[dump (len=%d)]\n", dump->len);
    dump_ctrl_free(dump);
}

int ynode_snprintf(char *buf, int buflen, ynode *node, int level)
{
    int len = -1;
    struct dump_ctrl *dump;
    if (!node)
        return -1;
    dump = dump_ctrl_new(NULL, 0, buf, buflen, level);
    if (!dump)
        return -1;
    dump_ctrl_dump_ynode(dump, node);
    len = dump->len;
    dump_ctrl_free(dump);
    return len;
}

int ynode_fprintf(FILE *fp, ynode *node, int level)
{
    int len = -1;
    struct dump_ctrl *dump;
    if (!node)
        return -1;
    dump = dump_ctrl_new(fp, 0, NULL, 0, level);
    if (!dump)
        return -1;
    dump_ctrl_dump_ynode(dump, node);
    len = dump->len;
    dump_ctrl_free(dump);
    return len;
}

int ynode_write(int fd, ynode *node, int level)
{
    int len = -1;
    struct dump_ctrl *dump;
    if (!node)
        return -1;
    dump = dump_ctrl_new(NULL, fd, NULL, 0, level);
    if (!dump)
        return -1;
    dump_ctrl_dump_ynode(dump, node);
    len = dump->len;
    dump_ctrl_free(dump);
    return len;
}

int ynode_printf(ynode *node, int level)
{
    return ynode_fprintf(NULL, node, level);
}

// ynode = ynode_fscanf(FILE)
// ynode = ynode_scanf(stdout)
// ynode = ynode_read(fp)
// ynode = ynode_sscanf(buffer)

// ynode = ydb_search(/path/to/resource)
// ynode = ydb_search("path", "to", "resource")
// ydb = ydb_top()
// ydb = ydb_open()
// ydb_close(ydb)
// ydb_connect(/path/to/resource) open communication channel
//   - permission requested: ro/wo/rw
// ydb_close(/path/to/resource)
// ydb_bind(/path/to/resource)
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
