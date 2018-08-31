#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

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

static char *dump_ctrl_keystr_debug(ynode *node)
{
    static char keystr[256];
    if (IS_SET(node->flags, YNODE_FLAG_KEY))
        snprintf(keystr, 256, "key: %s", node->key);
    else if (IS_SET(node->flags, YNODE_FLAG_ITER))
        snprintf(keystr, 256, "iter: %p", node->iter);
    else
        snprintf(keystr, 256, "key: none");
    keystr[255] = 0;
    return keystr;
}

static char *dump_ctrl_keystr(ynode *node)
{
    static char keystr[256];
    if (IS_SET(node->flags, YNODE_FLAG_KEY))
        snprintf(keystr, 256, "%s:", node->key);
    else if (IS_SET(node->flags, YNODE_FLAG_ITER))
        snprintf(keystr, 256, "-");
    else
        snprintf(keystr, 256, "%s", "");
    keystr[255] = 0;
    return keystr;
}

static int dump_ctrl_debug_ynode(struct dump_ctrl *cb, ynode *node)
{
    int indent = cb->level * 2;
    if(indent < 0)
        return YDB_OK;
    switch (node->type)
    {
    case YNODE_TYPE_DICT:
        switch (cb->type)
        {
        case DUMP_TYPE_FP:
            cb->len += fprintf(cb->fp, "%.*sdict {ptr:%p, %s, dict(count): %d, parent: %p}\n",
                               indent, space, node, dump_ctrl_keystr_debug(node), ytree_size(node->dict), node->parent);
            break;
        case DUMP_TYPE_FD:
            cb->len += dprintf(cb->fd, "%.*sdict {ptr:%p, %s, dict(count): %d, parent: %p}\n",
                               indent, space, node, dump_ctrl_keystr_debug(node), ytree_size(node->dict), node->parent);
            break;
        case DUMP_TYPE_STR:
            cb->len += snprintf((cb->buf + cb->len), (cb->buflen - cb->len),
                                "%.*sdict {ptr:%p, %s, dict(count): %d, parent: %p}\n",
                                indent, space, node, dump_ctrl_keystr_debug(node), ytree_size(node->dict), node->parent);
            if (cb->buflen <= cb->len)
            {
                cb->buf[cb->buflen - 1] = 0;
                return YDB_E_FULL_BUF;
            }
            break;
        default:
            assert(!YDB_E_DUMP_CB);
        }
        break;
    case YNODE_TYPE_LIST:
        switch (cb->type)
        {
        case DUMP_TYPE_FP:
            cb->len += fprintf(cb->fp, "%.*slist {ptr:%p, %s, list: %s, parent: %p}\n",
                               indent, space, node, dump_ctrl_keystr_debug(node), (ylist_empty(node->list)) ? "empty" : "not-empty",
                               node->parent);
            break;
        case DUMP_TYPE_FD:
            cb->len += dprintf(cb->fd, "%.*slist {ptr:%p, %s, list: %s, parent: %p}\n",
                               indent, space, node, dump_ctrl_keystr_debug(node), (ylist_empty(node->list)) ? "empty" : "not-empty",
                               node->parent);
            break;
        case DUMP_TYPE_STR:
            cb->len += snprintf((cb->buf + cb->len), (cb->buflen - cb->len),
                                "%.*slist {ptr:%p, %s, list: %s, parent: %p}\n",
                                indent, space, node, dump_ctrl_keystr_debug(node), (ylist_empty(node->list)) ? "empty" : "not-empty",
                                node->parent);
            if (cb->buflen <= cb->len)
            {
                cb->buf[cb->buflen - 1] = 0;
                return YDB_E_FULL_BUF;
            }
            break;
        default:
            assert(!YDB_E_DUMP_CB);
        }
        break;
    case YNODE_TYPE_VAL:
        switch (cb->type)
        {
        case DUMP_TYPE_FP:
            cb->len += fprintf(cb->fp, "%.*sval {ptr:%p, %s, value: %s, parent: %p}\n",
                               indent, space, node, dump_ctrl_keystr_debug(node), node->value, node->parent);
            break;
        case DUMP_TYPE_FD:
            cb->len += dprintf(cb->fd, "%.*sval {ptr:%p, %s, value: %s, parent: %p}\n",
                               indent, space, node, dump_ctrl_keystr_debug(node), node->value, node->parent);
            break;
        case DUMP_TYPE_STR:
            cb->len += snprintf((cb->buf + cb->len), (cb->buflen - cb->len),
                                "%.*sval {ptr:%p, %s, value: %s, parent: %p}\n",
                                indent, space, node, dump_ctrl_keystr_debug(node), node->value, node->parent);
            if (cb->buflen <= cb->len)
            {
                cb->buf[cb->buflen - 1] = 0;
                return YDB_E_FULL_BUF;
            }
            break;
        default:
            assert(!YDB_E_DUMP_CB);
        }
        break;
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    return YDB_OK;
}

static int dump_ctrl_print_ynode(struct dump_ctrl *dump, ynode *node)
{
    int indent = dump->level * 2;
    if(indent < 0)
        return YDB_OK;
    switch (node->type)
    {
    case YNODE_TYPE_DICT:
        switch (dump->type)
        {
        case DUMP_TYPE_FP:
            dump->len += fprintf(dump->fp, "%.*s%s\n", indent, space, dump_ctrl_keystr(node));
            break;
        case DUMP_TYPE_FD:
            dump->len += dprintf(dump->fd, "%.*s%s\n", indent, space, dump_ctrl_keystr(node));
            break;
        case DUMP_TYPE_STR:
            dump->len += snprintf((dump->buf + dump->len), (dump->buflen - dump->len),
                                "%.*s%s\n", indent, space, dump_ctrl_keystr(node));
            if (dump->buflen <= dump->len)
            {
                dump->buf[dump->buflen - 1] = 0;
                return YDB_E_FULL_BUF;
            }
            break;
        default:
            assert(!YDB_E_DUMP_CB);
        }
        break;
    case YNODE_TYPE_LIST:
        switch (dump->type)
        {
        case DUMP_TYPE_FP:
            dump->len += fprintf(dump->fp, "%.*s%s\n", indent, space, dump_ctrl_keystr(node));
            break;
        case DUMP_TYPE_FD:
            dump->len += dprintf(dump->fd, "%.*s%s\n", indent, space, dump_ctrl_keystr(node));
            break;
        case DUMP_TYPE_STR:
            dump->len += snprintf((dump->buf + dump->len), (dump->buflen - dump->len),
                                "%.*s%s\n", indent, space, dump_ctrl_keystr(node));
            if (dump->buflen <= dump->len)
            {
                dump->buf[dump->buflen - 1] = 0;
                return YDB_E_FULL_BUF;
            }
            break;
        default:
            assert(!YDB_E_DUMP_CB);
        }
        break;
    case YNODE_TYPE_VAL:
        switch (dump->type)
        {
        case DUMP_TYPE_FP:
            dump->len += fprintf(dump->fp, "%.*s%s %s\n", indent, space, dump_ctrl_keystr(node), node->value);
            break;
        case DUMP_TYPE_FD:
            dump->len += dprintf(dump->fd, "%.*s%s %s\n", indent, space, dump_ctrl_keystr(node), node->value);
            break;
        case DUMP_TYPE_STR:
            dump->len += snprintf((dump->buf + dump->len), (dump->buflen - dump->len),
                                "%.*s%s %s\n", indent, space, dump_ctrl_keystr(node), node->value);
            if (dump->buflen <= dump->len)
            {
                dump->buf[dump->buflen - 1] = 0;
                return YDB_E_FULL_BUF;
            }
            break;
        default:
            assert(!YDB_E_DUMP_CB);
        }
        break;
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    return YDB_OK;
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
    dump_ctrl_dump_ynode(dump, node);
    printf("\ndump len=%d\n", dump->len);
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
    dump_ctrl_dump_ynode(dump, node);
    printf("\ndump len=%d\n", dump->len);
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

// ynode_fprintf(FILE, ynode)
// ynode_printf(stdout, ynode)
// ynode_write(fp, ynode)
// ynode_sprintf(buffer, ynode)
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
