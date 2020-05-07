#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdbool.h>
#include <yaml.h>

#include "utf8.h"
#include "ylog.h"
#include "ystr.h"
#include "ylist.h"
#include "ytree.h"
#include "ytrie.h"
#include "ymap.h"

#include "ydb.h"
#include "ynode.h"

#define YNODE_LEVEL_MAX YDB_LEVEL_MAX

struct _yhook
{
    ynode *node;
    union {
        yhook_func0 func0;
        yhook_func1 func1;
        yhook_func2 func2;
        yhook_func3 func3;
        yhook_func4 func4;
        yhook_func5 func5;
        yhook_func2 func;
        yhook_suppressed_func0 agg_func0;
        yhook_suppressed_func1 agg_func1;
        yhook_suppressed_func2 agg_func2;
        yhook_suppressed_func3 agg_func3;
        yhook_suppressed_func4 agg_func4;
        yhook_suppressed_func5 agg_func5;
    };
    unsigned int flags;
    int user_num;
    void *user[];
};
typedef struct _yhook yhook;

// ynode flags
#define YNODE_FLAG_HASH 0x1
#define YNODE_FLAG_LIST 0x2

struct _ynode
{
    union {
        // const char *key;
        // ylist_iter *iter;
        ylist_iter *ilist;
        ymap_iter *imap;
        ytree_iter *itree;
        void *nkey;
    };
    union {
        ylist *list;
        ytree *map;
        ymap *omap;
        const char *value;
        void *nval;
    };
    node_type type;
    unsigned char flags;
    unsigned short origin;
    struct _ynode *parent;
    struct _ynode *meta; // for meta data
    struct _yhook *hook;
    const char *tag;
};

static char *ynode_type_str[] = {
    "!!none",
    "",
    "",
    "",
    "!!set",
    "!!imap",
    "!!omap",
};

static char *ynode_type_debug_str[] = {
    "!!none",
    "!!str",
    "!!seq",
    "!!map",
    "!!set",
    "!!imap",
    "!!omap",
};

#define SET_FLAG(flag, v) ((flag) = ((flag) | (v)))
#define UNSET_FLAG(flag, v) ((flag) = ((flag) & (~v)))
#define IS_SET(flag, v) ((flag) & (v))

static void yhook_delete(ynode *cur);

// delete ynode regardless of the detachment of the parent
static void ynode_free(ynode *node)
{
    if (!node)
        return;
    switch (node->type)
    {
    case YNODE_TYPE_VAL:
        if (node->value)
            yfree(node->value);
        break;
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_SET:
    case YNODE_TYPE_IMAP:
        ytree_destroy_custom(node->map, (user_free)ynode_free);
        break;
    case YNODE_TYPE_OMAP:
        ymap_destroy_custom(node->omap, (user_free)ynode_free);
        break;
    case YNODE_TYPE_LIST:
        ylist_destroy_custom(node->list, (user_free)ynode_free);
        break;
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    if (node->meta)
        ynode_free(node->meta);
    if (node->tag)
        yfree(node->tag);
    yhook_delete(node);
    free(node);
}

static int imap_cmp(char *a, char *b)
{
    int res = atoi(a) - atoi(b);
    if (res < 0 || res > 0)
        return res;
    return strcmp(a, b);
}

static void ynode_tag_ctrl(node_type *_type, const char **_tag)
{
    node_type type = *_type;
    const char *tag = *_tag;
    if (tag)
    {
        if (tag[0] == '!' && tag[1] == '!')
        {
            if (strcmp(tag, "!!map") == 0)
            {
                type = YNODE_TYPE_MAP;
                tag = NULL;
            }
            else if (strcmp(tag, "!!seq") == 0)
            {
                type = YNODE_TYPE_LIST;
                tag = NULL;
            }
            else if (strcmp(tag, "!!omap") == 0)
            {
                type = YNODE_TYPE_OMAP;
                tag = NULL;
            }
            else if (strcmp(tag, "!!set") == 0)
            {
                type = YNODE_TYPE_SET;
                tag = NULL;
            }
            else if (strcmp(tag, "!!imap") == 0)
            {
                type = YNODE_TYPE_IMAP;
                tag = NULL;
            }
            else if (strcmp(tag, "!!str") == 0)
            {
                type = YNODE_TYPE_VAL;
                tag = NULL;
            }
        }
    }
    *_type = type;
    *_tag = tag;
}

// create ynode
static ynode *ynode_new(node_type type, const char *tag, const char *value, int origin)
{
    ynode *node = malloc(sizeof(ynode));
    if (!node)
        return NULL;
    memset(node, 0x0, sizeof(ynode));
    ynode_tag_ctrl(&type, &tag);
    switch (type)
    {
    case YNODE_TYPE_VAL:
        node->value = ystrdup((char *)value);
        break;
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_SET:
        node->map = ytree_create((ytree_cmp)strcmp, (user_free)yfree);
        break;
    case YNODE_TYPE_IMAP:
        node->map = ytree_create((ytree_cmp)imap_cmp, (user_free)yfree);
        break;
    case YNODE_TYPE_OMAP:
        node->omap = ymap_create((ytree_cmp)strcmp, (user_free)yfree);
        break;
    case YNODE_TYPE_LIST:
        node->list = ylist_create();
        break;
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    if (!node->nval)
        goto _error;
    node->type = type;
    node->origin = origin;
    node->flags = 0x0;
    if (tag)
        node->tag = ystrdup((char *)tag);
    return node;
_error:
    free(node);
    return NULL;
}

// return parent after remove the node from the parent node.
static ynode *ynode_detach(ynode *node)
{
    ynode *parent;
    ynode *searched_node;
    if (!node)
        return NULL;
    if (!node->parent)
        return NULL;
    assert(node->nkey);
    parent = node->parent;
    switch (parent->type)
    {
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_SET:
    case YNODE_TYPE_IMAP:
        searched_node = ytree_delete(parent->map, ytree_key(node->itree));
        UNSET_FLAG(node->flags, YNODE_FLAG_HASH);
        assert(searched_node && YDB_E_NO_ENTRY);
        assert(searched_node == node && YDB_E_INVALID_PARENT);
        break;
    case YNODE_TYPE_OMAP:
        searched_node = ymap_delete(parent->omap, ymap_key(node->imap));
        UNSET_FLAG(node->flags, YNODE_FLAG_HASH);
        UNSET_FLAG(node->flags, YNODE_FLAG_LIST);
        assert(searched_node && YDB_E_NO_ENTRY);
        assert(searched_node == node && YDB_E_INVALID_PARENT);
        break;
    case YNODE_TYPE_LIST:
        ylist_erase(parent->list, node->ilist, NULL);
        UNSET_FLAG(node->flags, YNODE_FLAG_LIST);
        break;
    case YNODE_TYPE_VAL:
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    node->parent = NULL;
    node->nkey = NULL;
    return parent;
}

// insert the ynode to the parent, the key is used for map type.
// return old ynode that was being attached to the parent.
static ynode *ynode_attach(ynode *node, ynode *parent, const char *key)
{
    char *ykey;
    ynode *old = NULL;
    if (!node || !parent)
        return NULL;
    assert(parent->type != YNODE_TYPE_VAL);
    if (node->parent)
        ynode_detach(node);
    switch (parent->type)
    {
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_SET:
    case YNODE_TYPE_IMAP:
        ykey = (char *)ystrdup((char *)key);
        SET_FLAG(node->flags, YNODE_FLAG_HASH);
        node->itree = ytree_push(parent->map, ykey, node, (void **)&old);
        assert(node->itree);
        break;
    case YNODE_TYPE_OMAP:
        ykey = (char *)ystrdup((char *)key);
        SET_FLAG(node->flags, YNODE_FLAG_HASH);
        SET_FLAG(node->flags, YNODE_FLAG_LIST);
        old = ymap_insert_back(parent->omap, ykey, node);
        node->imap = ymap_find(parent->omap, ykey);
        assert(node->imap);
        break;
    case YNODE_TYPE_LIST:
        // ignore key.
        SET_FLAG(node->flags, YNODE_FLAG_LIST);
        node->ilist = ylist_push_back(parent->list, node);
        assert(node->ilist);
        break;
    case YNODE_TYPE_VAL:
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    node->parent = parent;
    return old;
}

void ynode_move_child(ynode *dest, ynode *src)
{
    ynode *n;
    if (!dest || !src)
        return;
    n = ynode_down(src);
    while (n)
    {
        const char *key;
        key = ystrdup((char *)ynode_key(n));
        ynode *old = ynode_attach(n, dest, key);
        yfree(key);
        ynode_free(old);
        n = ynode_down(src);
    }
}

// register the hook func to the target ynode.
ydb_res yhook_register(ynode *node, unsigned int flags, yhook_func func, int user_num, void *user[])
{
    yhook *hook;
    if (!node || !func)
        return YDB_E_INVALID_ARGS;
    if (IS_SET(flags, YNODE_LEAF_ONLY))
        return YDB_E_INVALID_ARGS;
    if (user_num > 5 || user_num < 0)
        return YDB_E_INVALID_ARGS;
    if (!user && user_num > 0)
        return YDB_E_INVALID_ARGS;

    hook = NULL;
    if (node->hook)
    {
        if (node->hook->user_num == user_num)
            hook = node->hook;
        else
            yhook_unregister(node);
    }

    if (!hook)
    {
        hook = malloc(sizeof(yhook) + sizeof(void *) * user_num);
        if (hook)
            memset(hook, 0x0, sizeof(yhook) + sizeof(void *) * user_num);
    }
    if (!hook)
        return YDB_E_NO_ENTRY;

    hook->flags = 0x0;
    if (IS_SET(flags, YNODE_VAL_ONLY))
        SET_FLAG(hook->flags, YNODE_VAL_ONLY);
    else
        UNSET_FLAG(hook->flags, YNODE_VAL_ONLY);

    if (IS_SET(flags, YNODE_SUPPRESS_HOOK))
        SET_FLAG(hook->flags, YNODE_SUPPRESS_HOOK);
    else
        UNSET_FLAG(hook->flags, YNODE_SUPPRESS_HOOK);

    hook->node = node;
    hook->func = func;
    hook->user_num = user_num;
    if (user_num > 0)
        memcpy(hook->user, user, sizeof(void *) * user_num);
    node->hook = hook;
    if (YLOG_SEVERITY_INFO)
    {
        int pathlen = 0;
        char *path = ynode_path(node, YDB_LEVEL_MAX, &pathlen);
        if (path)
        {
            ylog_info("write hook (%p) added to %s (%d)\n", func, path, pathlen);
            free(path);
        }
    }
    return YDB_OK;
}

// unregister the hook func from the target ynode.
// return user data registered with the hook.
void yhook_unregister(ynode *node)
{
    if (YLOG_SEVERITY_INFO && node->hook)
    {
        int pathlen = 0;
        char *path = ynode_path(node, YDB_LEVEL_MAX, &pathlen);
        if (path)
        {
            ylog_info("write hook (%p) deleted from %s (%d)\n", node->hook->func, path, pathlen);
            free(path);
        }
    }
    yhook_delete(node);
}

static void yhook_func_exec(yhook *hook, char op, ynode *cur, ynode *_new)
{
    assert(hook->func);
    if (YLOG_SEVERITY_INFO)
    {
        int pathlen = 0;
        char *path = ynode_path(hook->node, YDB_LEVEL_MAX, &pathlen);
        if (path)
        {
            ylog_info("write hook (%s) %s\n", path, hook ? "found" : "not found");
            free(path);
        }
    }
    if (IS_SET(hook->flags, YNODE_SUPPRESS_HOOK))
    {
        switch (hook->user_num)
        {
        case 0:
            hook->agg_func0(op, hook->node);
            break;
        case 1:
            hook->agg_func1(hook->user[0], op, hook->node);
            break;
        case 2:
            hook->agg_func2(hook->user[0], op, hook->node, hook->user[1]);
            break;
        case 3:
            hook->agg_func3(hook->user[0], op, hook->node, hook->user[1], hook->user[2]);
            break;
        case 4:
            hook->agg_func4(hook->user[0], op, hook->node, hook->user[1], hook->user[2], hook->user[3]);
            break;
        case 5:
            hook->agg_func5(hook->user[0], op, hook->node, hook->user[1], hook->user[2], hook->user[3], hook->user[4]);
            break;
        default:
            break;
        }
    }
    else
    {
        switch (hook->user_num)
        {
        case 0:
            hook->func0(op, hook->node, cur, _new);
            break;
        case 1:
            hook->func1(hook->user[0], op, hook->node, cur, _new);
            break;
        case 2:
            hook->func2(hook->user[0], op, hook->node, cur, _new, hook->user[1]);
            break;
        case 3:
            hook->func3(hook->user[0], op, hook->node, cur, _new, hook->user[1], hook->user[2]);
            break;
        case 4:
            hook->func4(hook->user[0], op, hook->node, cur, _new, hook->user[1], hook->user[2], hook->user[3]);
            break;
        case 5:
            hook->func5(hook->user[0], op, hook->node, cur, _new, hook->user[1], hook->user[2], hook->user[3], hook->user[4]);
            break;
        default:
            break;
        }
    }
}

static void yhook_push_for_pending(yhook *hook, ytree **hook_pool)
{
    if (!hook_pool)
        return;
    if (!(*hook_pool))
        *hook_pool = ytree_create(NULL, NULL);
    if (*hook_pool)
        ytree_insert(*hook_pool, hook->node, hook);
}

static int yhook_pre_run_for_delete(ynode *cur, ytree **hook_pool);
static int yhook_pre_run_for_delete_dict(void *key, void *data, void *addition)
{
    ynode *cur = data;
    ytree **hook_pool = addition;
    key = (void *)key;
    return yhook_pre_run_for_delete(cur, hook_pool);
}

static int yhook_pre_run_for_delete_list(void *data, void *addition)
{
    ynode *cur = data;
    ytree **hook_pool = addition;
    return yhook_pre_run_for_delete(cur, hook_pool);
}

// call the pre / post hook for deleting cur ynode.
static int yhook_pre_run_for_delete(ynode *cur, ytree **hook_pool)
{
    ydb_res res = YDB_OK;
    ynode *node;
    if (!cur)
        return YDB_E_NO_ENTRY;
    switch (cur->type)
    {
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_SET:
    case YNODE_TYPE_IMAP:
        res = ytree_traverse(cur->map, yhook_pre_run_for_delete_dict, hook_pool);
        break;
    case YNODE_TYPE_OMAP:
        res = ymap_traverse_order(cur->omap, yhook_pre_run_for_delete_dict, hook_pool);
        break;
    case YNODE_TYPE_LIST:
        res = ylist_traverse(cur->list, yhook_pre_run_for_delete_list, hook_pool);
        break;
    case YNODE_TYPE_VAL:
        res = YDB_OK;
        break;
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    node = cur;
    while (node)
    {
        yhook *hook = node->hook;
        if (hook)
        {
            if (IS_SET(hook->flags, YNODE_SUPPRESS_HOOK))
            {
                yhook_push_for_pending(hook, hook_pool);
                break;
            }
            if (cur->type == YNODE_TYPE_VAL || !IS_SET(hook->flags, YNODE_VAL_ONLY))
                yhook_func_exec(hook, YHOOK_OP_DELETE, cur, NULL);
            break;
        }
        node = node->parent;
    }

    return res;
}

static yhook *yhook_pre_run(char op, ynode *parent, ynode *cur, ynode *new)
{
    yhook *hook;
    if (op != YHOOK_OP_CREATE && op != YHOOK_OP_REPLACE)
        return NULL;
    if (new)
    {
        hook = new->hook;
        if (hook)
        {
            if (IS_SET(hook->flags, YNODE_SUPPRESS_HOOK))
                return hook;
            if ((cur && cur->type == YNODE_TYPE_VAL) ||
                (new && new->type == YNODE_TYPE_VAL) ||
                !IS_SET(hook->flags, YNODE_VAL_ONLY))
                yhook_func_exec(hook, op, cur, new);
            return NULL;
        }
    }
    while (parent)
    {
        hook = parent->hook;
        if (hook)
        {
            if (IS_SET(hook->flags, YNODE_SUPPRESS_HOOK))
                return hook;
            if ((cur && cur->type == YNODE_TYPE_VAL) ||
                (new && new->type == YNODE_TYPE_VAL) ||
                !IS_SET(hook->flags, YNODE_VAL_ONLY))
                yhook_func_exec(hook, op, cur, new);
            break;
        }
        parent = parent->parent;
    }
    return NULL;
}

static void yhook_post_run(char op, ynode *cur, bool end_of_run, ytree **hook_pool)
{
    yhook *hook;
    if (!hook_pool || !(*hook_pool))
        return;
    if (op != YHOOK_OP_DELETE)
    {
        if (end_of_run)
        {
            while (cur)
            {
                hook = ytree_delete(*hook_pool, (void *)cur);
                if (hook)
                    yhook_func_exec(hook, op, NULL, NULL);
                cur = cur->parent;
            }
        }
        else
        {
            hook = ytree_delete(*hook_pool, (void *)cur);
            if (hook)
                yhook_func_exec(hook, op, NULL, NULL);
        }
    }

    if (end_of_run)
    {
        ytree_iter *iter = ytree_top(*hook_pool);
        while (iter)
        {
            hook = ytree_data(iter);
            if (hook)
                yhook_func_exec(hook, op, NULL, NULL);
            ytree_remove(*hook_pool, iter, NULL);
            iter = ytree_top(*hook_pool);
        }
        ytree_destroy(*hook_pool);
        *hook_pool = NULL;
    }
}

static void yhook_delete(ynode *cur)
{
    if (!cur || !cur->hook)
        return;
    free(cur->hook);
    cur->hook = NULL;
}

static void yhook_copy(ynode *dest, ynode *src)
{
    yhook *hook;
    if (!dest || !src)
        return;
    if (!src->hook)
        return;
    yhook_delete(dest);
    hook = malloc(sizeof(yhook) + sizeof(void *) * src->hook->user_num);
    if (!hook)
        return;
    memcpy(hook, src->hook, sizeof(yhook) + sizeof(void *) * src->hook->user_num);
    dest->hook = hook;
    hook->node = dest;
}

struct _ynode_record
{
    enum
    {
        RECORD_TYPE_FP,
        RECORD_TYPE_FD,
        RECORD_TYPE_STR,
    } type;
    FILE *fp;
    int fd;
    char *buf;
    int buflen;
    int len;
#define DUMP_FLAG_DEBUG 0x1
    unsigned int flags;
    int end_level;
    int start_level;
    int level;
    int indent;
};
typedef struct _ynode_record ynode_record;

#define S10 "          "
static char *space = S10 S10 S10 S10 S10 S10 S10 S10 S10 S10;

struct _ynode_record *ynode_record_new(FILE *fp, int fd, char *buf, int buflen, int start_level, int end_level)
{
    struct _ynode_record *record;
    record = malloc(sizeof(struct _ynode_record));
    if (!record)
        return NULL;
    if (fp)
        record->type = RECORD_TYPE_FP;
    else if (fd)
        record->type = RECORD_TYPE_FD;
    else if (buf && buflen > 0)
        record->type = RECORD_TYPE_STR;
    else
    {
        record->type = RECORD_TYPE_FP;
        fp = stdout;
    }
    record->fp = fp;
    record->fd = fd;
    record->buf = buf;
    record->buflen = buflen;
    record->len = 0;
    record->flags = 0x0;
    record->end_level = end_level;
    record->start_level = start_level;
    record->level = 0;
    record->indent = 0;
    return record;
}

struct _ynode_record *ynode_record_new_debug(FILE *fp, int fd, char *buf, int buflen, int start_level, int end_level)
{
    struct _ynode_record *record;
    record = ynode_record_new(fp, fd, buf, buflen, start_level, end_level);
    if (!record)
        return NULL;
    record->flags = DUMP_FLAG_DEBUG;
    return record;
}

void ynode_record_free(struct _ynode_record *record)
{
    if (record)
        free(record);
}

int _ynode_record_print(struct _ynode_record *record, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    switch (record->type)
    {
    case RECORD_TYPE_FP:
        record->len += vfprintf(record->fp, format, args);
        break;
    case RECORD_TYPE_FD:
        record->len += vdprintf(record->fd, format, args);
        break;
    case RECORD_TYPE_STR:
        record->len += vsnprintf((record->buf + record->len), (record->buflen - record->len), format, args);
        if (record->buflen <= record->len)
        {
            record->buf[record->buflen - 1] = 0;
            return YDB_E_FULL_BUF;
        }
        break;
    default:
        assert(record->type && !YDB_E_TYPE_ERR);
    }
    va_end(args);
    return YDB_OK;
}

static int _ynode_record_debug_ynode(struct _ynode_record *record, ynode *node)
{
    ydb_res res;
    int indent = record->indent;
    if (indent < 0)
        return YDB_OK;

    // print indent
    res = _ynode_record_print(record, "%.*s", indent, space);
    if (res)
        return res;

    res = _ynode_record_print(record, "%p ", node);
    if (res)
        return res;

    // print key
    if (IS_SET(node->flags, YNODE_FLAG_HASH))
    {
        int is_new;
        char *key = to_yaml(ynode_key(node), -1, &is_new, 0);
        res = _ynode_record_print(record, "{key: %s,", key);
        if (is_new)
            free(key);
    }
    else if (IS_SET(node->flags, YNODE_FLAG_LIST))
        res = _ynode_record_print(record, "{key: none,");
    else
        res = _ynode_record_print(record, "{key: none,");
    if (res)
        return res;

    // tag
    if (node->tag)
        res = _ynode_record_print(record, " tag: %s, ", node->tag);

    // print flags
    switch (node->type)
    {
    case YNODE_TYPE_VAL:
    {
        int is_new;
        char *value = to_yaml(node->value, -1, &is_new, 0);
        res = _ynode_record_print(record, " value: %s,", value);
        if (is_new)
            free(value);
        break;
    }
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_SET:
    case YNODE_TYPE_IMAP:
    {
        res = _ynode_record_print(record, " %s: num=%d,",
                                  ynode_type_debug_str[node->type], ytree_size(node->map));
        break;
    }
    case YNODE_TYPE_OMAP:
    {
        res = _ynode_record_print(record, " omap: num=%d,", ymap_size(node->omap));
        break;
    }
    case YNODE_TYPE_LIST:
    {
        res = _ynode_record_print(record, " list: num=%d,", ylist_size(node->list));
        break;
    }
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    if (res)
        return res;
    res = _ynode_record_print(record, " parent: %p, origin: %d}\n", node->parent, node->origin);
    return res;
}

static int _ynode_record_print_ynode(struct _ynode_record *record, ynode *node)
{
    int only_val = 0;
    ydb_res res;
    int indent;
    if (!record)
        return YDB_OK;
    indent = record->indent;
    if (indent < 0)
        return YDB_OK;

    // print indent
    res = _ynode_record_print(record, "%.*s", indent, space);
    if (res)
        return res;
    // print key
    if (IS_SET(node->flags, YNODE_FLAG_HASH))
    {
        int is_new;
        char *key;
        assert(node->parent);
        key = to_yaml(ynode_key(node), -1, &is_new, 0);
        if (node->parent->type == YNODE_TYPE_OMAP)
            res = _ynode_record_print(record, "- %s:", key);
        else if (node->parent->type == YNODE_TYPE_SET)
            res = _ynode_record_print(record, "? %s", key);
        else
            res = _ynode_record_print(record, "%s:", key);
        if (is_new)
            free(key);
    }
    else if (IS_SET(node->flags, YNODE_FLAG_LIST))
    {
        res = _ynode_record_print(record, "-");
    }
    else
    {
        only_val = 1;
    }

    if (res)
        return res;

    // print value
    if (node->type == YNODE_TYPE_VAL)
    {
        int is_new;
        char *value = to_yaml(node->value, indent, &is_new, 0);
        res = _ynode_record_print(record, "%s%s%s%s\n",
                                  only_val ? "" : " ",
                                  node->tag ? node->tag : "",
                                  node->tag ? " " : "",
                                  value);
        if (is_new)
            free(value);
    }
    else
    {
        if (node->tag)
            res = _ynode_record_print(record, " %s\n", node->tag);
        else
            res = _ynode_record_print(record, " %s\n", ynode_type_str[node->type]);
    }
    return res;
}

static int _ynode_record_dump_childen(struct _ynode_record *record, ynode *node);
static int _ynode_record_traverse_dict(void *key, void *data, void *addition)
{
    struct _ynode_record *record = addition;
    ynode *node = data;
    key = (void *)key;
    return _ynode_record_dump_childen(record, node);
}

static int _ynode_record_traverse_list(void *data, void *addition)
{
    struct _ynode_record *record = addition;
    ynode *node = data;
    return _ynode_record_dump_childen(record, node);
}

static int _ynode_record_dump_parent(struct _ynode_record *record, ynode *node)
{
    ydb_res res = YDB_OK;
    ylist *parents = ylist_create();
    int start_level = record->start_level;
    int end_level = (record->end_level < 0) ? record->end_level : 0;
    node = node->parent;
    start_level++;
    while (node && start_level <= end_level)
    {
        ylist_push_back(parents, node);
        node = node->parent;
        start_level++;
    }

    while (!ylist_empty(parents))
    {
        node = ylist_pop_back(parents);
        if (IS_SET(record->flags, DUMP_FLAG_DEBUG))
            res = _ynode_record_debug_ynode(record, node);
        else
            res = _ynode_record_print_ynode(record, node);
        // printf("\ndump len=%d\n", record->len);
        if (res)
            break;
        record->indent++;
    }
    record->start_level = (record->start_level < 0) ? 0 : record->start_level;
    ylist_destroy(parents);
    return res;
}

static int _ynode_record_dump_childen(struct _ynode_record *record, ynode *node)
{
    ydb_res res = YDB_OK;
    if (record->end_level < 0)
        return res;
    record->end_level--;
    // printf("record->start_level %d, record->level %d\n", record->start_level, record->level);
    if (record->start_level <= record->level)
    {
        if (IS_SET(record->flags, DUMP_FLAG_DEBUG))
            res = _ynode_record_debug_ynode(record, node);
        else
            res = _ynode_record_print_ynode(record, node);
        if (res)
            return res;
        record->indent++;
    }

    record->level++;
    switch (node->type)
    {
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_SET:
    case YNODE_TYPE_IMAP:
        res = ytree_traverse(node->map, _ynode_record_traverse_dict, record);
        break;
    case YNODE_TYPE_OMAP:
        res = ymap_traverse_order(node->omap, _ynode_record_traverse_dict, record);
        break;
    case YNODE_TYPE_LIST:
        res = ylist_traverse(node->list, _ynode_record_traverse_list, record);
        break;
    case YNODE_TYPE_VAL:
        break;
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    record->level--;
    if (record->start_level <= record->level)
        record->indent--;
    record->end_level++;
    return res;
}

void ynode_dump_node(FILE *fp, int fd, char *buf, int buflen, ynode *node, int start_level, int end_level)
{
    struct _ynode_record *record;
    if (!node)
        return;
    if (start_level > end_level)
        return;
    record = ynode_record_new_debug(fp, fd, buf, buflen, start_level, end_level);
    if (!record)
        return;
    if (start_level < end_level)
        _ynode_record_print(record, "\n[record (start_level=%d, end_level=%d)]\n", start_level, end_level);
    _ynode_record_dump_parent(record, node);
    _ynode_record_dump_childen(record, node);
    if (start_level < end_level)
        _ynode_record_print(record, "[record (len=%d)]\n", record->len);
    ynode_record_free(record);
}

int ynode_printf_to_buf(char *buf, int buflen, ynode *node, int start_level, int end_level)
{
    int len = -1;
    struct _ynode_record *record;
    if (!node)
        return -1;
    if (start_level > end_level)
        return 0;
    record = ynode_record_new(NULL, 0, buf, buflen, start_level, end_level);
    if (!record)
        return -1;
    _ynode_record_dump_parent(record, node);
    _ynode_record_dump_childen(record, node);
    len = record->len;
    ynode_record_free(record);
    return len;
}

int ynode_printf_to_fp(FILE *fp, ynode *node, int start_level, int end_level)
{
    int len = -1;
    struct _ynode_record *record;
    if (!node)
        return -1;
    if (start_level > end_level)
        return 0;
    record = ynode_record_new(fp, 0, NULL, 0, start_level, end_level);
    if (!record)
        return -1;
    _ynode_record_dump_parent(record, node);
    _ynode_record_dump_childen(record, node);
    len = record->len;
    ynode_record_free(record);
    return len;
}

int ynode_printf_to_fd(int fd, ynode *node, int start_level, int end_level)
{
    int len = -1;
    struct _ynode_record *record;
    if (!node)
        return -1;
    if (start_level > end_level)
        return 0;
    record = ynode_record_new(NULL, fd, NULL, 0, start_level, end_level);
    if (!record)
        return -1;
    _ynode_record_dump_parent(record, node);
    _ynode_record_dump_childen(record, node);
    len = record->len;
    ynode_record_free(record);
    return len;
}

int ynode_printf(ynode *node, int start_level, int end_level)
{
    return ynode_printf_to_fp(NULL, node, start_level, end_level);
}

int ynode_fprintf_meta(FILE *fp, ynode *node)
{
    if (!node)
        return -1;
    if (!node->meta)
        return 0;
    return ynode_printf(node->meta, 1, 24);
}

struct _ynode_log
{
    FILE *fp;
    char *buf;
    size_t buflen;
    ynode *top;
    ylist *printed_nodes;
    bool isdump;
};

struct _ynode_log *ynode_log_open(ynode *top, FILE *dumpfp)
{
    struct _ynode_log *log = malloc(sizeof(struct _ynode_log));
    if (!log)
        return NULL;
    memset(log, 0x0, sizeof(struct _ynode_log));
    if (!dumpfp)
        log->fp = open_memstream(&log->buf, &log->buflen);
    else
    {
        log->fp = dumpfp;
        log->isdump = true;
    }
    if (!log->fp)
    {
        free(log);
        return NULL;
    }
    log->top = top;
    return log;
}

void ynode_log_close(struct _ynode_log *log, char **buf, size_t *buflen)
{
    if (!log)
        return;
    if (log->printed_nodes)
        ylist_destroy(log->printed_nodes);
    if (log->fp && !log->isdump)
        fclose(log->fp);
    if (buf && buflen)
    {
        *buf = log->buf;
        *buflen = log->buflen;
        free(log);
    }
    else
    {
        if (log->buf)
            free(log->buf);
        free(log);
    }
}

static void ynode_log_print(struct _ynode_log *log, bool is_del, ynode *_cur, ynode *_new)
{
    int indent = 0;
    ynode *n, *last;
    ylist *nodes;
    if (!log)
        return;
    nodes = ylist_create();
    if (!nodes)
        return;
    if (_new)
    {
        n = _new;
        if (_cur && (log->top == _cur))
        {
            log->top = _new;
        }
    }
    else if (_cur)
        n = _cur;
    else
        return;

    // the ancestors of the current node
    while (n)
    {
        if (log->top == n)
            break;
        ylist_push_front(nodes, n);
        n = n->parent;
    };

    // compare the current ancestors with the last printed ancestors.
    ylist_iter *iter = NULL;
#if 0 // compare nodes from the head to the tail.
    iter = ylist_first(nodes);
    last = ylist_pop_front(log->printed_nodes);
    while (last)
    {
        n = ylist_data(iter);
        if (n != last)
            break;
        indent++;
        iter = ylist_next(nodes, iter);
        last = ylist_pop_front(log->printed_nodes);
    }
#else
    if (ylist_size(log->printed_nodes) == 0)
    {
        iter = ylist_first(nodes);
        goto node_print;
    }
    else if (ylist_size(log->printed_nodes) < ylist_size(nodes))
    {
        int count = ylist_size(nodes);
        iter = ylist_last(nodes);
        while (ylist_size(log->printed_nodes) < count)
        {
            iter = ylist_prev(nodes, iter);
            count--;
        }
    }
    else
    {
        while (ylist_size(log->printed_nodes) > ylist_size(nodes))
            ylist_pop_back(log->printed_nodes);
        iter = ylist_last(nodes);
    }

    for (; !ylist_done(nodes, iter); iter = ylist_prev(nodes, iter))
    {
        last = ylist_back(log->printed_nodes);
        n = ylist_data(iter);
        if (!last || !n)
            break;
        if (last == n)
        {
            iter = ylist_next(nodes, iter);
            break;
        }
        ylist_pop_back(log->printed_nodes);
    }

    indent = ylist_size(log->printed_nodes);
    if (ylist_done(nodes, iter))
        iter = ylist_first(nodes);
node_print:
#endif

    for (; !ylist_done(nodes, iter);)
    {
        int only_val = 0;
        n = ylist_data(iter);
        // print nodes and cur to fp using indent
        fprintf(log->fp, "%.*s", indent, space);
        // print key
        if (IS_SET(n->flags, YNODE_FLAG_HASH))
        {
            int is_new;
            char *key;
            assert(n->parent);
            key = to_yaml(ynode_key(n), -1, &is_new, 0);
            if (n->parent->type == YNODE_TYPE_OMAP)
                fprintf(log->fp, "- %s:", key);
            else if (n->parent->type == YNODE_TYPE_SET)
                fprintf(log->fp, "? %s", key);
            else
                fprintf(log->fp, "%s:", key);
            if (is_new)
                free(key);
        }
        else if (IS_SET(n->flags, YNODE_FLAG_LIST))
            fprintf(log->fp, "-");
        else
            only_val = 1;

        iter = ylist_next(nodes, iter);

        // !ydb!delete tag processing
        if (is_del && ylist_done(nodes, iter))
        {
            fprintf(log->fp, "%s!ydb!delete\n", only_val ? "" : " ");
            break;
        }

        // print value
        if (n->type == YNODE_TYPE_VAL)
        {
            int is_new;
            char *value = to_yaml(n->value, indent, &is_new, 0);
            fprintf(log->fp, "%s%s%s%s\n",
                    only_val ? "" : " ",
                    n->tag ? n->tag : "",
                    n->tag ? " " : "",
                    value);
            if (is_new)
                free(value);
        }
        else
        {
            if (n->tag)
                fprintf(log->fp, " %s\n", n->tag);
            else
                fprintf(log->fp, " %s\n", ynode_type_str[n->type]);
        }
        indent++;
    }

    // update the printed_nodes
    ylist_destroy(log->printed_nodes);
    log->printed_nodes = nodes;
    return;
}

int ydb_log_err_yaml(yaml_parser_t *parser, char *buf, int buflen)
{

    /* Display a parser error message. */
    switch (parser->error)
    {
    case YAML_MEMORY_ERROR:
        ylog_error("mem err: not enough memory for parsing\n");
        break;

    case YAML_READER_ERROR:
        if (parser->problem_value != -1)
        {
            ylog_error("reader error: %s: #%X at %zd\n", parser->problem,
                       parser->problem_value, parser->problem_offset);
        }
        else
        {
            ylog_error("reader error: %s at %zu\n", parser->problem,
                       parser->problem_offset);
        }
        break;

    case YAML_SCANNER_ERROR:
        if (parser->context)
        {
            ylog_error("scanner error: %s at line %zu, column %zu\n",
                       parser->context,
                       parser->context_mark.line + 1, parser->context_mark.column + 1);
            ylog_error("%s at line %zu, column %zu\n",
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        else
        {
            ylog_error("scanner error: %s at line %zu, column %zu\n",
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        break;

    case YAML_PARSER_ERROR:
        if (parser->context)
        {
            ylog_error("parser error: %s at line %zu, column %zu\n",
                       parser->context,
                       parser->context_mark.line + 1, parser->context_mark.column + 1);
            ylog_error("%s at line %zu, column %zu\n",
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        else
        {
            ylog_error("parser error: %s at line %zu, column %zu\n",
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        break;

    case YAML_COMPOSER_ERROR:
        if (parser->context)
        {
            ylog_error("composer error: %s at line %zu, column %zu\n",
                       parser->context,
                       parser->context_mark.line + 1, parser->context_mark.column + 1);
            ylog_error("%s at line %zu, column %zu\n",
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
            ylog_error("\n");
        }
        else
        {
            ylog_error("composer error: %s at line %zu, column %zu\n",
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        break;

    default:
        /* Couldn't happen. */
        ylog_error("internal error\n");
        break;
    }

    if (buf)
    {
        ylog_error("buf:: \n%.*s\n", buflen, buf);
    }
    else if (parser->raw_buffer.start)
    {
        // ylog_error("parser->raw->start=%d\n", parser->raw_buffer.start - parser->raw_buffer.start);
        // ylog_error("parser->raw->point=%d\n", parser->raw_buffer.pointer - parser->raw_buffer.start);
        // ylog_error("parser->raw->end  =%d\n", parser->raw_buffer.end - parser->raw_buffer.start);
        // ylog_error("parser->raw->last =%d\n", parser->raw_buffer.last - parser->raw_buffer.start);
        // ylog_error("parser->buf->start=%d\n", parser->buffer.start - parser->buffer.start);
        // ylog_error("parser->buf->point=%d\n", parser->buffer.pointer- parser->buffer.start);
        // ylog_error("parser->buf->end  =%d\n", parser->buffer.end - parser->buffer.start);
        // ylog_error("parser->buf->last =%d\n", parser->buffer.last - parser->buffer.start);
        if (parser->raw_buffer.last && parser->raw_buffer.last - parser->raw_buffer.start > 0)
        {
            ylog_error("raw_bffer:: \n%.*s\n",
                       parser->raw_buffer.last - parser->raw_buffer.start,
                       parser->raw_buffer.start);
        }
        else if (parser->buffer.last)
        {
            ylog_error("buffer:: \n%.*s\n",
                       parser->buffer.last - parser->buffer.start,
                       parser->buffer.start);
        }
    }
    return 0;
}

#define YAML_TOKEN_NAME(NAME) #NAME
char *yaml_token_str[] = {
    YAML_TOKEN_NAME(YAML_NO_TOKEN),

    YAML_TOKEN_NAME(YAML_STREAM_START_TOKEN),
    YAML_TOKEN_NAME(YAML_STREAM_END_TOKEN),

    YAML_TOKEN_NAME(YAML_VERSION_DIRECTIVE_TOKEN),
    YAML_TOKEN_NAME(YAML_TAG_DIRECTIVE_TOKEN),
    YAML_TOKEN_NAME(YAML_DOCUMENT_START_TOKEN),
    YAML_TOKEN_NAME(YAML_DOCUMENT_END_TOKEN),

    YAML_TOKEN_NAME(YAML_BLOCK_SEQUENCE_START_TOKEN),
    YAML_TOKEN_NAME(YAML_BLOCK_MAPPING_START_TOKEN),
    YAML_TOKEN_NAME(YAML_BLOCK_END_TOKEN),

    YAML_TOKEN_NAME(YAML_FLOW_SEQUENCE_START_TOKEN),
    YAML_TOKEN_NAME(YAML_FLOW_SEQUENCE_END_TOKEN),
    YAML_TOKEN_NAME(YAML_FLOW_MAPPING_START_TOKEN),
    YAML_TOKEN_NAME(YAML_FLOW_MAPPING_END_TOKEN),

    YAML_TOKEN_NAME(YAML_BLOCK_ENTRY_TOKEN),
    YAML_TOKEN_NAME(YAML_FLOW_ENTRY_TOKEN),
    YAML_TOKEN_NAME(YAML_KEY_TOKEN),
    YAML_TOKEN_NAME(YAML_VALUE_TOKEN),

    YAML_TOKEN_NAME(YAML_ALIAS_TOKEN),
    YAML_TOKEN_NAME(YAML_ANCHOR_TOKEN),
    YAML_TOKEN_NAME(YAML_TAG_TOKEN),
    YAML_TOKEN_NAME(YAML_SCALAR_TOKEN),
};

static char *ynode_strpbrk(char *src, char *src_end, char *stop)
{
    char *delimiter;
    if (strlen(stop) == 1)
    {
        for (; src != src_end; src++)
            if (*src == stop[0])
                return src;
        return NULL;
    }

    for (; src != src_end; src++)
    {
        for (delimiter = stop; *delimiter != 0; delimiter++)
        {
            if (*src == *delimiter)
                return src;
        }
    }
    return NULL;
}

struct ynode_query_data
{
    yaml_parser_t *parser;
    int num_of_query;
};

static int ynod_yaml_query_handler(void *data, unsigned char *buffer, size_t size,
                                   size_t *size_read)
{
    int len;
    char *start, *cur, *end, *buf;
    struct ynode_query_data *qd = data;
    yaml_parser_t *parser = (yaml_parser_t *)qd->parser;

    if (parser->input.string.current == parser->input.string.end)
    {
        *size_read = 0;
        return 1;
    }
    start = (char *)parser->input.string.start;
    cur = (char *)parser->input.string.current;
    end = (char *)parser->input.string.end;
    buf = (char *)buffer;

    char *specifier = ynode_strpbrk(cur, end, "%");
    while (specifier)
    {
        char *pre = specifier - 1;
        len = specifier - cur;
        if (size <= (size_t)len)
            break;
        memcpy(buf, cur, len);
        cur += len;
        size -= len;
        buf += len;

        if (specifier == start || isspace(*pre))
        {
            // add ""
            char *s_end = ynode_strpbrk(specifier, end, " \n:,}]\r\t\f\v");
            len = s_end - cur;
            if (size <= (len + 3) || len <= 0)
                goto _done;
            // copy
            sprintf(buf, "+%03d", qd->num_of_query);
            buf += 4;
            size -= 4;
            qd->num_of_query++;
            // *buf = '"';
            // buf++;
            // size--;

            memcpy(buf, cur, len);
            cur += len;
            buf += len;
            size -= len;

            // *buf = '"';
            // buf++;
            // size--;
        }
        else
        {
            if (size <= 1)
                break;
            *buf = *cur;
            cur++;
            buf++;
            size--;
        }

        specifier = ynode_strpbrk(cur, end, "%");
    }

    len = end - cur;
    if (len <= 0)
        goto _done;
    if (size <= (size_t)len)
        len = size;
    memcpy(buf, cur, len);
    cur += len;
    buf += len;
    size -= len;

_done:
    // printf("buff='%s'\n", buffer);
    *size_read = buf - ((char *)buffer);
    parser->input.string.current = (const unsigned char *)cur;
    return 1;
}

#define YNODE_SCAN_FAIL(cond, root_cause)                           \
    if (cond)                                                       \
    {                                                               \
        char *failpath = ynode_path(top, YDB_LEVEL_MAX, NULL);      \
        res = root_cause;                                           \
        if (failpath)                                               \
        {                                                           \
            ylog(YLOG_ERROR, "%s(%s)\n", failpath, key ? key : ""); \
            free(failpath);                                         \
        }                                                           \
        ylog(YLOG_ERROR, "'%s': %s\n", #cond, ydb_res_str(res));    \
        break;                                                      \
    }

#define CLEAR_YSTR(v) \
    do                \
    {                 \
        if (v)        \
            yfree(v); \
        v = NULL;     \
    } while (0)

static ynode *ynode_new_and_attach(node_type type, const char *tag, const char *key, char *value, int origin, ynode *parent)
{
    ynode *new, *old = NULL;
    ynode_tag_ctrl(&type, &tag);

    if (parent && key)
        old = ynode_find_child(parent, key);
    if (old)
    {
        if (old->type != type)
            goto create_new;
        if (type == YNODE_TYPE_VAL)
            goto create_new;
        if (old->tag && tag)
        {
            if (strcmp(tag, old->tag) != 0)
                goto create_new;
        }
        old->origin = origin;
        return old;
    }
create_new:
    new = ynode_new(type, tag, value, origin);
    if (new)
    {
        old = ynode_attach(new, parent, key);
        ynode_free(old);
    }
    return new;
}

static ynode *ynode_set_tag_directive(ynode *node, char *key, char *value)
{
    ynode *p;
    if (!node)
        node = ynode_new(YNODE_TYPE_MAP, "!!map", NULL, 0);
    if (!node)
        return NULL;
    if (!node->meta)
        node->meta = ynode_new(YNODE_TYPE_MAP, "!!map", NULL, 0);
    if (!node->meta)
        return node;
    p = ynode_find_child(node->meta, "%TAG");
    if (!p)
        p = ynode_new_and_attach(YNODE_TYPE_MAP, NULL, "%TAG", NULL, 0, node->meta);
    if (p)
        ynode_new_and_attach(YNODE_TYPE_VAL, NULL, key, value, 0, p);
    return node;
}

static ynode *ynode_set_meta(ynode *base_node, ynode *meta_node)
{
    ynode *m;
    if (!base_node)
        base_node = ynode_new(YNODE_TYPE_MAP, "!!map", NULL, 0);
    if (!base_node)
        return NULL;
    if (!base_node->meta)
        base_node->meta = ynode_new(YNODE_TYPE_MAP, "!!map", NULL, 0);
    if (!base_node->meta)
        return base_node;
    m = ynode_find_child(base_node->meta, "$META");
    ynode_detach(meta_node);
    if (!m)
    {
        ynode_attach(meta_node, base_node->meta, "$META");
    }
    else
    {
        ynode_merge(m, meta_node, NULL);
        ynode_free(meta_node);
    }
    return base_node;
}

ydb_res ynode_scan(FILE *fp, char *buf, int buflen, int origin, ynode **n, int *queryform)
{
    ydb_res res = YDB_OK;
    int level = 0;
    ynode *top = NULL;
    ynode *new = NULL;
    const char *key = NULL;
    char *scalar = NULL;
    struct ynode_query_data qdata;
    yaml_token_type_t stored_token;
    yaml_token_type_t last_token;
    yaml_parser_t parser;
    yaml_token_t token;
    yaml_token_type_t ignore_block_map = YAML_NO_TOKEN;
    bool scalar_next = false;
    bool token_save = false;
    bool multiple_anchor_reference = false;
    ytrie *anchors = NULL;
    ytree *meta = NULL;
    ytree *dirv = NULL;
    const char *tag = NULL;
    node_type type;

    if ((!fp && !buf) || !n)
    {
        res = YDB_E_INVALID_ARGS;
        return res;
    }

    /* Initialize parser */
    if (!yaml_parser_initialize(&parser))
    {
        ydb_log_err_yaml(&parser, buf, buflen);
        yaml_parser_delete(&parser);
        res = YDB_E_YAML_INIT_FAILED;
        return res;
    }

    if (queryform)
    {
        /* Set input buf and handler */
        parser.input.string.start = (const unsigned char *)buf;
        parser.input.string.current = (const unsigned char *)buf;
        parser.input.string.end = (const unsigned char *)buf + buflen;
        qdata.parser = &parser;
        qdata.num_of_query = 0;
        yaml_parser_set_input(&parser, ynod_yaml_query_handler, &qdata);
    }
    else
    {
        if (fp)
            yaml_parser_set_input_file(&parser, fp);
        else
            yaml_parser_set_input_string(&parser, (const unsigned char *)buf, (size_t)buflen);
    }

    stored_token = YAML_NO_TOKEN;
    last_token = YAML_NO_TOKEN;

    do
    {
        yaml_parser_scan(&parser, &token);
        if (!token.type)
        {
            ydb_log_err_yaml(&parser, buf, buflen);
            res = YDB_E_YAML_PARSING_FAILED;
            break;
        }

        if (token.type == YAML_BLOCK_END_TOKEN ||
            token.type == YAML_FLOW_MAPPING_END_TOKEN ||
            token.type == YAML_FLOW_SEQUENCE_END_TOKEN)
            level--;
        if (level < 0)
        {
            ylog_error("Invalid Block or Flow token\n");
        }
        ylog_debug("%d_%.*s%s\n", level, (level>0)?level:0, space, yaml_token_str[token.type]);

        if (token.type == YAML_BLOCK_SEQUENCE_START_TOKEN ||
            token.type == YAML_BLOCK_MAPPING_START_TOKEN ||
            token.type == YAML_FLOW_SEQUENCE_START_TOKEN ||
            token.type == YAML_FLOW_MAPPING_START_TOKEN)
            level++;

        token_save = true;

        switch (token.type)
        {
        case YAML_KEY_TOKEN:
            if (top->type == YNODE_TYPE_LIST)
            { // create map to the list parent
                new = ynode_new_and_attach(YNODE_TYPE_MAP, "!!map", NULL, NULL, origin, top);
                YNODE_SCAN_FAIL(!new, YDB_E_MEM_ALLOC);
                ylog_debug("%d_%.*s%s %s (created [pairs])\n", level, level, space,
                           ynode_type_debug_str[new->type], "no-key");
                top = new;
            }
            if (key)
            {
                ylog_debug("%d_%.*s%s: null\n",
                           level, level, space, key ? key : "");
                new = ynode_new_and_attach(YNODE_TYPE_VAL, tag, key, NULL, origin, top);
                YNODE_SCAN_FAIL(!new, YDB_E_MEM_ALLOC);
                CLEAR_YSTR(tag);
                CLEAR_YSTR(key);
            }
            break;
        case YAML_VALUE_TOKEN:
            break;
        /* Block delimeters */
        case YAML_BLOCK_SEQUENCE_START_TOKEN:
        case YAML_FLOW_SEQUENCE_START_TOKEN:
        case YAML_BLOCK_MAPPING_START_TOKEN:
        case YAML_FLOW_MAPPING_START_TOKEN:
        {
            if (key && strcmp(key, "<<") == 0)
            {
                // ignore flow sequence end token
                ignore_block_map = YAML_FLOW_SEQUENCE_END_TOKEN;
                multiple_anchor_reference = true;
                break;
            }
            if (top && top->type == YNODE_TYPE_OMAP)
            {
                // ignore block end token
                if (stored_token == YAML_BLOCK_ENTRY_TOKEN &&
                    token.type == YAML_BLOCK_MAPPING_START_TOKEN)
                {
                    ignore_block_map = YAML_BLOCK_END_TOKEN;
                    break;
                }
            }
            // don't create top if YAML document restarted.
            if (level == 1 && top)
                if (token.type == YAML_BLOCK_MAPPING_START_TOKEN ||
                    token.type == YAML_FLOW_MAPPING_START_TOKEN)
                    break;
            if (token.type == YAML_BLOCK_MAPPING_START_TOKEN ||
                token.type == YAML_FLOW_MAPPING_START_TOKEN)
                type = YNODE_TYPE_MAP;
            else
                type = YNODE_TYPE_LIST;

            new = ynode_new_and_attach(type, tag, key, NULL, origin, top);
            YNODE_SCAN_FAIL(!new, YDB_E_MEM_ALLOC);
            ylog_debug("%d_%.*s%s %s (created %p)\n", level, level, space,
                       ynode_type_debug_str[new->type], key ? key : "no-key", new);
            CLEAR_YSTR(tag);
            CLEAR_YSTR(key);
            top = new;
            break;
        }
        case YAML_BLOCK_ENTRY_TOKEN: // -
        case YAML_FLOW_ENTRY_TOKEN:  // ,
            if (scalar_next || (last_token == YAML_FLOW_SEQUENCE_START_TOKEN))
            {
                ylog_debug("%d_%.*s%s: null\n",
                           level, level, space, key ? key : "");
                new = ynode_new_and_attach(YNODE_TYPE_VAL, tag, key, NULL, origin, top);
                YNODE_SCAN_FAIL(!new, YDB_E_MEM_ALLOC);
                CLEAR_YSTR(tag);
                CLEAR_YSTR(key);
                top = (top == NULL) ? new : top;
            }
            scalar_next = true;
            break;
        case YAML_BLOCK_END_TOKEN:
        case YAML_FLOW_MAPPING_END_TOKEN:
        case YAML_FLOW_SEQUENCE_END_TOKEN:
            if (ignore_block_map == token.type)
            {
                ignore_block_map = YAML_NO_TOKEN;
                multiple_anchor_reference = false;
                break;
            }
            if (key || scalar_next)
            {
                ylog_debug("%d_%.*s%s: null\n", level, level, space, key ? key : "");
                new = ynode_new_and_attach(YNODE_TYPE_VAL, tag, key, NULL, origin, top);
                YNODE_SCAN_FAIL(!new, YDB_E_MEM_ALLOC);
                CLEAR_YSTR(tag);
                CLEAR_YSTR(key);
            }
            if (top == NULL)
            {
                ydb_log_err_yaml(&parser, buf, buflen);
                ylog_error("CRASH is avoided. origin(fd: %d) should be checked...\n", origin);
                res = YDB_E_YAML_PARSING_FAILED;
                // [FIXME]
                if (ylog_severity > YLOG_ERROR)
                    ylog_severity = YLOG_ERROR;
            }
            else
            {
                top = (top->parent) ? top->parent : top;
            }
            break;
        case YAML_SCALAR_TOKEN:
            token_save = false;
            scalar_next = false;
            scalar = (char *)token.data.scalar.value;
            switch (stored_token)
            {
            case YAML_KEY_TOKEN:
                ylog_debug("%d_%.*s%s (new key)\n", level, level, space, scalar);
                key = ystrdup(scalar);
                if (strcmp(key, "$META") == 0)
                {
                    if (!meta)
                        meta = ytree_create((ytree_cmp)strcmp, NULL);
                    if (meta)
                    {
                        char *path;
                        path = ynode_path_with_pre_postfix(
                            top, YDB_LEVEL_MAX, NULL, NULL, scalar);
                        if (path)
                            ytree_insert_custom(meta, path, path, free);
                    }
                }
                break;
            case YAML_STREAM_START_TOKEN:
            case YAML_DOCUMENT_START_TOKEN:
            case YAML_VALUE_TOKEN:
                ylog_debug("%d_%.*s%s%s%s (val)\n", level, level, space,
                           key ? key : "", key ? ": " : "", scalar);
                new = ynode_new_and_attach(YNODE_TYPE_VAL, tag, key, scalar, origin, top);
                YNODE_SCAN_FAIL(!new, YDB_E_MEM_ALLOC);
                CLEAR_YSTR(tag);
                CLEAR_YSTR(key);
                top = (top == NULL) ? new : top;
                break;
            case YAML_BLOCK_SEQUENCE_START_TOKEN:
            case YAML_BLOCK_MAPPING_START_TOKEN:
            case YAML_BLOCK_ENTRY_TOKEN:
            case YAML_FLOW_SEQUENCE_START_TOKEN:
            case YAML_FLOW_MAPPING_START_TOKEN:
            case YAML_FLOW_ENTRY_TOKEN:
                if (top->type == YNODE_TYPE_MAP ||
                    top->type == YNODE_TYPE_SET ||
                    top->type == YNODE_TYPE_IMAP ||
                    top->type == YNODE_TYPE_OMAP)
                {
                    if (!key)
                    {
                        ylog_debug("%d_%.*s%s (new key)\n", level, level, space, scalar);
                        key = ystrdup(scalar);
                        scalar = NULL;
                    }
                }
                ylog_debug("%d_%.*s%s%s%s (val)\n",
                           level, level, space,
                           key ? key : "", key ? ": " : "", scalar);
                new = ynode_new_and_attach(YNODE_TYPE_VAL, tag, key, scalar, origin, top);
                YNODE_SCAN_FAIL(!new, YDB_E_MEM_ALLOC);
                CLEAR_YSTR(tag);
                CLEAR_YSTR(key);
                top = (top == NULL) ? new : top;
                break;
            default:
                break;
            }
            break;
        case YAML_DOCUMENT_START_TOKEN:
        case YAML_STREAM_START_TOKEN:
        case YAML_DOCUMENT_END_TOKEN:
        case YAML_STREAM_END_TOKEN:
            break;
        case YAML_TAG_TOKEN:
            ylog_debug("handle=%s suffix=%s\n", token.data.tag.handle, token.data.tag.suffix);
            CLEAR_YSTR(tag);
            tag = ystrnew("%s%s", (char *)token.data.tag.handle, (char *)token.data.tag.suffix);
            token_save = false;
            break;
        /* Others */
        case YAML_ANCHOR_TOKEN:
        {
            char *path;
            char *anchor;
            int pathlen = 0, n = 0;
            char anchor_path[256];
            if (!anchors)
            {
                anchors = ytrie_create();
                YNODE_SCAN_FAIL(!new, YDB_E_MEM_ALLOC);
            }
            anchor = (char *)token.data.anchor.value;
            path = ynode_path(top, YDB_LEVEL_MAX, &pathlen);
            if (path)
            {
                n = snprintf(anchor_path, sizeof(anchor_path), "%s", path);
                free(path);
            }
            if (key)
                n += snprintf(anchor_path + n, sizeof(anchor_path) - n, "/%s", key);
            else if (ynode_size(top) >= 0)
                n += snprintf(anchor_path + n, sizeof(anchor_path) - n, "/%d", ynode_size(top));
            else
            {
                ylog_debug("anchor (%s) ignored ...\n", anchor);
                break;
            }
            path = ytrie_insert(anchors, anchor, strlen(anchor), (void *)ystrdup(anchor_path));
            if (path) // free old anchor
                yfree(path);
            ylog_debug("anchor (%s: %s) stored\n", anchor, anchor_path);
            token_save = false;
            break;
        }
        case YAML_ALIAS_TOKEN:
        {
            char *path;
            char *alias;
            ynode *copy, *old = NULL;
            alias = (char *)token.data.alias.value;
            if (!anchors)
                break;
            path = ytrie_search(anchors, alias, strlen(alias));
            if (!path)
            {
                ylog_debug("no anchor for (%s) ...\n", alias);
                break;
            }

            copy = ynode_copy(ynode_search(ynode_top(top), path));
            if (!copy)
                break;

            if ((key && strcmp(key, "<<") == 0) || multiple_anchor_reference)
            {
                ynode_move_child(top, copy);
                if (multiple_anchor_reference)
                    scalar_next = false;
                ynode_free(copy); // remove remains.
            }
            else
            {
                old = ynode_attach(copy, top, key);
                ynode_free(old);
            }
            ylog_debug("anchor (%s, %s) loaded (key %s)\n", alias, path, key);
            CLEAR_YSTR(key);
            token_save = false;
            break;
        }
        case YAML_TAG_DIRECTIVE_TOKEN:
            if (!dirv)
                dirv = ytree_create((ytree_cmp)strcmp, free);
            if (dirv)
            {
                void *handle = (void *)strdup((char *)token.data.tag_directive.handle);
                void *prefix = (void *)strdup((char *)token.data.tag_directive.prefix);
                ytree_insert_custom(dirv, handle, prefix, free);
            }
        case YAML_VERSION_DIRECTIVE_TOKEN:
            token_save = false;
        default:
            break;
        }
        last_token = token.type;
        if (token_save)
            stored_token = token.type;
        if (res)
            break;

        if (token.type != YAML_STREAM_END_TOKEN)
            yaml_token_delete(&token);
    } while (token.type != YAML_STREAM_END_TOKEN);
    yaml_token_delete(&token);
    yaml_parser_delete(&parser);

    if (res)
    {
        ynode_free(ynode_top(top));
        top = NULL;
    }
    else
    {
        ytree_iter *i;
        if (dirv)
        {
            for (i = ytree_first(dirv); i; i = ytree_next(dirv, i))
                ynode_set_tag_directive(top, ytree_key(i), ytree_data(i));
            ytree_destroy_custom(dirv, free);
        }
        if (meta)
        {
            for (i = ytree_first(meta); i; i = ytree_next(meta, i))
            {
                char *path = ytree_data(i);
                ynode *m = ynode_search(top, path);
                if (m && m->parent)
                {
                    ynode_set_meta(m->parent, m);
                    // if (YLOG_SEVERITY_INFO && (m->parent != top))
                    //     ynode_printf(m->parent->meta, 1, 24);
                }
            }
            ytree_destroy_custom(meta, free);
        }
        // if ((YLOG_SEVERITY_INFO))
        //     if (top && top->meta)
        //         ynode_printf(top->meta, 1, 24);
    }
    if (anchors)
        ytrie_destroy_custom(anchors, (user_free)yfree);
    // ynode_dump(top, 0, 24);
    CLEAR_YSTR(tag);
    CLEAR_YSTR(key);
    *n = top;
    if (queryform)
        *queryform = qdata.num_of_query;
    return res;
}

ydb_res ynode_scanf_from_fp(FILE *fp, ynode **n)
{
    if (fp && n)
        return ynode_scan(fp, NULL, 0, 0, n, 0);
    // return ynode_scan(fp, NULL, 0, fileno(fp), n, 0);
    return YDB_E_INVALID_ARGS;
}

ydb_res ynode_scanf(ynode **n)
{
    return ynode_scan(stdin, NULL, 0, STDIN_FILENO, n, 0);
}

ydb_res ynode_scanf_from_fd(int fd, ynode **n)
{
    int dup_fd = dup(fd);
    FILE *fp = fdopen(dup_fd, "r");
    ydb_res res = ynode_scan(fp, NULL, 0, fd, n, 0);
    if (fp)
        fclose(fp);
    return res;
}

ydb_res ynode_scanf_from_buf(char *buf, int buflen, int origin, ynode **n)
{
    ydb_res res;
    if (!buf || buflen < 0)
        return YDB_E_INVALID_ARGS;
    res = ynode_scan(NULL, buf, buflen, origin, n, 0);
    return res;
}

void ynode_remove(ynode *n)
{
    ynode_detach(n);
    ynode_free(n);
}

ynode *ynode_find_child(ynode *node, const char *key)
{
    if (!node)
        return NULL;
    switch (node->type)
    {
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_SET:
    case YNODE_TYPE_IMAP:
        return ytree_search(node->map, (char *)key);
    case YNODE_TYPE_OMAP:
        return ymap_search(node->omap, (char *)key);
    case YNODE_TYPE_LIST:
    {
        int count = 0;
        ylist_iter *iter;
        int index = -1;
        // if (strspn(key, "0123456789") != strlen(key))
        //     return NULL;
        index = atoi(key);
        if (index < 0)
            return NULL;
        for (iter = ylist_first(node->list);
             !ylist_done(node->list, iter);
             iter = ylist_next(node->list, iter))
        {
            if (index == count)
                return ylist_data(iter);
            count++;
        }
        return NULL;
    }
    case YNODE_TYPE_VAL:
        return NULL;
    default:
        assert(YDB_E_TYPE_ERR);
    }
    return NULL;
}

#define PATH_DELIMITER "/="
#define PATH_IGNORE_START "'{[(\""
#define PATH_IGNORE_END "\"}])'"

static char get_pair_delimiter(char c)
{
    switch (c)
    {
    case '"':
        return c;
    case '\'':
        return c;
    case '{':
        return '}';
    case '[':
        return ']';
    case '(':
        return ')';
    case '<':
        return '>';
    case '}':
        return '{';
    case ']':
        return '[';
    case ')':
        return '(';
    case '>':
        return '<';
    }
    return c;
}

ylist *ynode_path_tokenize(char *path, char **val)
{
    char *key;
    char *token;
    bool is_val = false;
    bool stop = false;
    ylist *keylist;
    if (!path)
        return NULL;
    keylist = ylist_create();
    if (!keylist)
        return NULL;
    token = strpbrk(path, PATH_DELIMITER PATH_IGNORE_START);
    while (token)
    {
        while (*token != '=' && *token != '/')
        {
            char c = *token;
            token += 1;
            token = strchr(token, get_pair_delimiter(c));
            // token = strpbrk(token, PATH_IGNORE_END);
            if (!token)
            {
                stop = true;
                break;
            }
            token += 1;
            token = strpbrk(token, PATH_DELIMITER PATH_IGNORE_START);
            if (!token)
            {
                stop = true;
                break;
            }
        }
        if (stop)
            break;

        if (is_val)
        {
            token = NULL;
            break;
        }
        else
        {
            int len = token - path;
            if (len > 0)
            {
                key = to_string((const char *)path, len, NULL);
                if (!key)
                    key = strndup(path, len);
                ylist_push_back(keylist, key);
            }
        }
        if (*token == '=')
        {
            is_val = true;
        }
        path = token + 1;
        token = strpbrk(path, PATH_DELIMITER PATH_IGNORE_START);
    }

    if (path[0] && !token)
    {
        if (is_val)
        {
            if (val)
            {
                *val = to_string(path, 0, NULL);
                if (!*val)
                    *val = strndup(path, 0);
            }
        }
        else
        {
            key = to_string((const char *)path, 0, NULL);
            if (!key)
                key = strdup(path);
            ylist_push_back(keylist, key);
        }
    }

    // if (ylist_empty(keylist))
    // {
    //     ylist_destroy(keylist);
    //     return NULL;
    // }

    if (YLOG_SEVERITY_DEBUG)
    {
        int n;
        char buf[512];
        n = snprintf(buf, sizeof(buf), "{");
        ylist_iter *iter = ylist_first(keylist);
        for (; !ylist_done(keylist, iter); iter = ylist_next(keylist, iter))
        {
            n += snprintf(buf + n, sizeof(buf) - n, " %s", (char *)ylist_data(iter));
        }
        n += snprintf(buf + n, sizeof(buf) - n, " }");
        ylog_debug("path token (%d): %s\n", ylist_size(keylist), buf);
    }
    return keylist;
}

ynode *ynode_search_best(ynode *base, char *path, int *matched)
{
    ylist *keylist;
    ynode *found;
    char *key;
    keylist = ynode_path_tokenize(path, NULL);
    if (!keylist)
        return NULL;

    if (matched)
        *matched = 1;
    found = base;
    key = ylist_pop_front(keylist);
    while (key)
    {
        ynode *node;
        node = ynode_find_child(found, key);
        free(key);
        if (node)
            found = node;
        else
        {
            if (matched)
                *matched = 0;
            break;
        }
        key = ylist_pop_front(keylist);
    }
    ylist_destroy_custom(keylist, free);
    return found;
}

// lookup the ynode in the path
ynode *ynode_search(ynode *base, char *path)
{
    int matched = 0;
    ynode *found;
    found = ynode_search_best(base, path, &matched);
    if (found && matched)
        return found;
    return NULL;
}

// return ynodes' type
int ynode_type(ynode *node)
{
    if (node)
        return node->type;
    return -1;
}

// return ynodes' tag
const char *ynode_tag(ynode *node)
{
    if (!node)
        return NULL;
    if (node->tag)
        return node->tag;
    switch (node->type)
    {
    case YNODE_TYPE_VAL:
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_IMAP:
    case YNODE_TYPE_OMAP:
    case YNODE_TYPE_LIST:
        return ynode_type_debug_str[node->type];
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    return NULL;
}

// return the ynode has a value or children
int ynode_empty(ynode *node)
{
    if (!node)
        return 1;
    switch (node->type)
    {
    case YNODE_TYPE_VAL:
        return (node->value) ? 0 : 1;
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_SET:
    case YNODE_TYPE_IMAP:
        return (ytree_size(node->map) <= 0) ? 1 : 0;
    case YNODE_TYPE_OMAP:
        return (ymap_size(node->omap) <= 0) ? 1 : 0;
    case YNODE_TYPE_LIST:
        return ylist_empty(node->list);
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    return 1;
}

// return the number of chilren
int ynode_size(ynode *node)
{
    if (!node)
        return -1;
    switch (node->type)
    {
    case YNODE_TYPE_VAL:
        return 0;
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_SET:
    case YNODE_TYPE_IMAP:
        return ytree_size(node->map);
    case YNODE_TYPE_OMAP:
        return ymap_size(node->omap);
    case YNODE_TYPE_LIST:
        return ylist_size(node->list);
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    return 0;
}

// return ynodes' value if that is a leaf.
const char *ynode_value(ynode *node)
{
    if (node && node->type == YNODE_TYPE_VAL)
        return node->value;
    return NULL;
}

// return ynodes' key if that has a hash key.
const char *ynode_key(ynode *node)
{
    unsigned int type;
    if (!node)
        return NULL;
    type = IS_SET(node->flags, (YNODE_FLAG_HASH | YNODE_FLAG_LIST));
    switch (type)
    {
    case YNODE_FLAG_HASH:
        return ytree_key(node->itree);
    case (YNODE_FLAG_HASH | YNODE_FLAG_LIST):
        return ymap_key(node->imap);
    case YNODE_FLAG_LIST:
    default:
        return NULL;
    }
}

// return ynodes' index if the nodes' parent is a list.
int ynode_index(ynode *node)
{
    if (!node || !node->parent)
        return -1;
    if (node->parent->type == YNODE_TYPE_LIST)
    {
        int index = 0;
        ylist_iter *iter;
        for (iter = ylist_first(node->parent->list);
             !ylist_done(node->parent->list, iter);
             iter = ylist_next(node->parent->list, iter))
        {
            if (node->ilist == iter)
                return index;
            index++;
        }
    }
    return -1;
}

// return ynodes' origin
int ynode_origin(ynode *node)
{
    if (!node)
        return 0;
    return node->origin;
}

// return the top node of the ynode.
ynode *ynode_top(ynode *node)
{
    while (node)
    {
        if (!node->parent)
            break;
        node = node->parent;
    }
    return node;
}

// return the parent node of the ynode.
ynode *ynode_up(ynode *node)
{
    if (node)
        return node->parent;
    return NULL;
}

// return the first child node of the ynode.
ynode *ynode_down(ynode *node)
{
    if (!node)
        return NULL;
    switch (node->type)
    {
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_SET:
    case YNODE_TYPE_IMAP:
        return ytree_data(ytree_first(node->map));
    case YNODE_TYPE_OMAP:
        return ymap_data(ymap_first(node->omap));
    case YNODE_TYPE_LIST:
        return ylist_front(node->list);
    case YNODE_TYPE_VAL:
    default:
        break;
    }
    return NULL;
}

// return the previous sibling node of the ynode.
ynode *ynode_prev(ynode *node)
{
    if (!node || !node->parent)
        return NULL;
    switch (node->parent->type)
    {
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_SET:
    case YNODE_TYPE_IMAP:
        return ytree_data(ytree_prev(node->parent->map, node->itree));
    case YNODE_TYPE_OMAP:
        return ymap_data(ymap_prev(node->parent->omap, node->imap));
    case YNODE_TYPE_LIST:
        return ylist_data(ylist_prev(node->parent->list, node->ilist));
    case YNODE_TYPE_VAL:
    default:
        break;
    }
    return NULL;
}

// return the next sibling node of the ynode.
ynode *ynode_next(ynode *node)
{
    if (!node || !node->parent)
        return NULL;
    switch (node->parent->type)
    {
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_SET:
    case YNODE_TYPE_IMAP:
        return ytree_data(ytree_next(node->parent->map, node->itree));
    case YNODE_TYPE_OMAP:
        return ymap_data(ymap_next(node->parent->omap, node->imap));
    case YNODE_TYPE_LIST:
        return ylist_data(ylist_next(node->parent->list, node->ilist));
    case YNODE_TYPE_VAL:
    default:
        break;
    }
    return NULL;
}

// return the first sibling node of the ynode.
ynode *ynode_first(ynode *node)
{
    if (!node || !node->parent)
        return NULL;
    switch (node->parent->type)
    {
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_SET:
    case YNODE_TYPE_IMAP:
        return ytree_data(ytree_first(node->parent->map));
    case YNODE_TYPE_OMAP:
        return ymap_data(ymap_first(node->parent->omap));
    case YNODE_TYPE_LIST:
        return ylist_front(node->parent->list);
    case YNODE_TYPE_VAL:
    default:
        break;
    }
    return NULL;
}

// return the last sibling node of the ynode.
ynode *ynode_last(ynode *node)
{
    if (!node || !node->parent)
        return NULL;
    switch (node->parent->type)
    {
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_SET:
    case YNODE_TYPE_IMAP:
        return ytree_data(ytree_last(node->parent->map));
    case YNODE_TYPE_OMAP:
        return ymap_data(ymap_last(node->parent->omap));
    case YNODE_TYPE_LIST:
        return ylist_back(node->parent->list);
    case YNODE_TYPE_VAL:
    default:
        break;
    }
    return NULL;
}

int ynode_path_fprintf(FILE *fp, ynode *node, int level)
{
    if (node && level > 0)
    {
        int len, curlen;
        len = ynode_path_fprintf(fp, node->parent, level - 1);
        if (IS_SET(node->flags, YNODE_FLAG_HASH))
        {
            int is_new;
            char *key = to_yaml(ynode_key(node), -1, &is_new, 1);
            curlen = fprintf(fp, "/%s", key);
            if (is_new)
                free(key);
            if (curlen <= 0)
                return len;
            return len + curlen;
        }
        else if (IS_SET(node->flags, YNODE_FLAG_LIST))
        {
            int index = ynode_index(node);
            if (index < 0)
                return len;
            curlen = fprintf(fp, "/%d", index);
            if (curlen <= 0)
                return len;
            return len + curlen;
        }
        else
        {
            return len;
        }
    }
    return 0;
}

int ynode_level(ynode *top, ynode *node)
{
    ynode *n;
    int level = 0;
    if (!node)
        return 0;
    n = node;
    while (n)
    {
        if (n == top)
            break;
        n = n->parent;
        level++;
    }
    return level;
}

char *ynode_path(ynode *node, int level, int *pathlen)
{
    char *buf = NULL;
    size_t buflen = 0;
    FILE *fp;
    if (!node)
        return NULL;
    fp = open_memstream(&buf, &buflen);
    ynode_path_fprintf(fp, node, level);
    if (fp)
        fclose(fp);
    if (buf && buflen > 0)
    {
        if (pathlen)
            *pathlen = buflen;
        return buf;
    }
    if (buf)
        free(buf);
    return strdup("/");
}

char *ynode_path_with_pre_postfix(ynode *node, int level, int *pathlen, char *prefix, char *postfix)
{
    char *buf = NULL;
    size_t buflen = 0;
    FILE *fp;
    if (!node)
        return NULL;
    fp = open_memstream(&buf, &buflen);
    if (fp && prefix)
    {
        if (prefix[0] == '/')
            fprintf(fp, "%s", prefix);
        else
            fprintf(fp, "/%s", prefix);
    }
    ynode_path_fprintf(fp, node, level);
    if (fp && postfix)
    {
        if (postfix[0] == '/')
            fprintf(fp, "%s", postfix);
        else
            fprintf(fp, "/%s", postfix);
    }
    if (fp)
        fclose(fp);
    if (buf && buflen > 0)
    {
        if (pathlen)
            *pathlen = buflen;
        return buf;
    }
    if (buf)
        free(buf);
    return strdup("/");
}

char *ynode_path_and_val(ynode *node, int level, int *pathlen)
{
    char *buf = NULL;
    size_t buflen = 0;
    FILE *fp;
    if (!node)
        return NULL;
    fp = open_memstream(&buf, &buflen);
    ynode_path_fprintf(fp, node, level);
    if (node->type == YNODE_TYPE_VAL)
        fprintf(fp, "=%s", node->value);
    if (fp)
        fclose(fp);
    if (buf && buflen > 0)
    {
        if (pathlen)
            *pathlen = buflen;
        return buf;
    }
    if (buf)
        free(buf);
    return strdup("/");
}

static char ynode_op_get(ynode *cur, ynode *new)
{
    if (!cur && !new)
        return YHOOK_OP_NONE;
    else if (!cur && new)
        return YHOOK_OP_CREATE;
    else if (cur && !new)
        return YHOOK_OP_DELETE;
    else
    {
        // !ydb!delete tag processing
        if (new->tag && strcmp(new->tag, "!ydb!delete") == 0)
            return YHOOK_OP_DELETE;
        else if (cur->type == new->type)
        {
            if (cur->type == YNODE_TYPE_VAL)
            {
                if (strcmp(cur->value, new->value) == 0)
                    return YHOOK_OP_NONE;
                return YHOOK_OP_REPLACE;
            }
            return YHOOK_OP_NONE;
        }
        else
        {
            // Both are equal if map, set, imap and omap.
            if (cur->type >= YNODE_TYPE_MAP && new->type >= YNODE_TYPE_MAP)
                return YHOOK_OP_NONE;
            return YHOOK_OP_REPLACE;
        }
    }
}

char *yhook_op_str(char op)
{
    switch (op)
    {
    case YHOOK_OP_CREATE:
        return "create";
    case YHOOK_OP_REPLACE:
        return "replace";
    case YHOOK_OP_DELETE:
        return "delete";
    case YHOOK_OP_NONE:
        return "none";
    default:
        return "???";
    }
}

static ynode *ynode_control(ynode *cur, ynode *src, ynode *parent, const char *key, ytree **hook_pool, ynode_log *log)
{
    yhook *hook;
    ytree *hpool = NULL;
    ynode *new = NULL;
    bool start_point = false;
    char op;
    if (parent)
    {
        if (cur && cur->parent != parent)
            assert(!YDB_E_INVALID_PARENT);
        switch (parent->type)
        {
        case YNODE_TYPE_LIST:
            key = NULL;
            break;
        case YNODE_TYPE_MAP:
        case YNODE_TYPE_SET:
        case YNODE_TYPE_IMAP:
            if (!cur && key)
                cur = ytree_search(parent->map, (void *)key);
            break;
        case YNODE_TYPE_OMAP:
            if (!cur && key)
                cur = ymap_search(parent->omap, (void *)key);
            break;
        case YNODE_TYPE_VAL:
            return NULL;
        default:
            assert(!YDB_E_TYPE_ERR);
        }
    }

    op = ynode_op_get(cur, src);

    if (op == YHOOK_OP_CREATE || op == YHOOK_OP_REPLACE)
    {
        new = ynode_new(src->type, src->tag, src->value, src->origin);
        if (!new)
            return NULL;
        yhook_copy(new, cur);
    }
    else if (op == YHOOK_OP_DELETE)
    {
        src = NULL;
    }
    else if (op == YHOOK_OP_NONE)
    {
        // update origin for value nodes
        if (src->type == YNODE_TYPE_VAL && cur->origin != 0)
            cur->origin = src->origin;
        new = cur;
    }

    if (YLOG_SEVERITY_INFO)
    {
        char *path = ynode_path_and_val(parent, YNODE_LEVEL_MAX, NULL);
        switch (op)
        {
        case YHOOK_OP_CREATE:
            ylog_info("create %s to %s\n", key ? key : "-", path ? path : "top");
            break;
        case YHOOK_OP_REPLACE:
            ylog_info("replace %s in %s\n", key ? key : "-", path ? path : "top");
            break;
        case YHOOK_OP_DELETE:
            ylog_info("delete %s from %s\n", key ? key : "-", path ? path : "top");
            break;
        default:
            break;
        }
        if (path)
            free(path);
    }

    if (!hook_pool)
    {
        hook_pool = &hpool;
        start_point = true;
    }

    switch (op)
    {
    case YHOOK_OP_CREATE:
        ynode_attach(new, parent, key);
        hook = yhook_pre_run(op, parent, cur, new);
        if (hook)
            yhook_push_for_pending(hook, hook_pool);
        ynode_log_print(log, false, cur, new);
        break;
    case YHOOK_OP_REPLACE:
        ynode_attach(new, parent, key);
        hook = yhook_pre_run(op, parent, cur, new);
        if (hook)
            yhook_push_for_pending(hook, hook_pool);
        ynode_log_print(log, false, cur, new);
        break;
    case YHOOK_OP_DELETE:
        yhook_pre_run_for_delete(cur, hook_pool);
        ynode_log_print(log, true, cur, new);
        break;
    case YHOOK_OP_NONE:
    default:
        break;
    }

    if (src)
    {
        switch (src->type)
        {
        case YNODE_TYPE_MAP:
        case YNODE_TYPE_SET:
        case YNODE_TYPE_IMAP:
        {
            ytree_iter *iter = ytree_first(src->map);
            for (; iter != NULL; iter = ytree_next(src->map, iter))
            {
                ynode *src_child = ytree_data(iter);
                ynode *cur_child = ynode_find_child(new, ynode_key(src_child));
                ynode_control(cur_child, src_child, new, ynode_key(src_child), hook_pool, log);
            }
            break;
        }
        case YNODE_TYPE_OMAP:
        {
            ymap_iter *iter = ymap_first(src->omap);
            for (; iter != NULL; iter = ymap_next(src->omap, iter))
            {
                ynode *src_child = ymap_data(iter);
                ynode *cur_child = ynode_find_child(new, ynode_key(src_child));
                ynode_control(cur_child, src_child, new, ynode_key(src_child), hook_pool, log);
            }
            break;
        }
        case YNODE_TYPE_LIST:
        {
            ylist_iter *iter;
            for (iter = ylist_first(src->list);
                 !ylist_done(src->list, iter);
                 iter = ylist_next(src->list, iter))
            {
                ynode *src_child = ylist_data(iter);
                ynode_control(NULL, src_child, new, NULL, hook_pool, log);
            }
            break;
        }
        case YNODE_TYPE_VAL:
            break;
        default:
            assert(!YDB_E_TYPE_ERR);
        }
    }

    switch (op)
    {
    case YHOOK_OP_CREATE:
        yhook_post_run(YHOOK_OP_MERGE, new, start_point, hook_pool);
        break;
    case YHOOK_OP_REPLACE:
        yhook_post_run(YHOOK_OP_MERGE, new, start_point, hook_pool);
        ynode_free(cur);
        break;
    case YHOOK_OP_DELETE:
        yhook_post_run(YHOOK_OP_DELETE, cur, start_point, hook_pool);
        ynode_detach(cur);
        ynode_free(cur);
        break;
    case YHOOK_OP_NONE:
        yhook_post_run(YHOOK_OP_MERGE, cur, start_point, hook_pool);
        break;
    default:
        break;
    }
    return new;
}

// get the src nodes' data using the log (ynode_log).
// return the number of value nodes printed to the log (ynode_log).
int ynode_get_with_origin(ynode *src, int origin, ynode_log *log)
{
    int n = 0;
    if (!src)
        return 0;
    if (src->type == YNODE_TYPE_VAL)
    {
        // doesn't print the value according to origin.
        if (src->origin != origin && origin >= 0)
            return 0;
        n += 1;
    }

    ynode_log_print(log, false, src, NULL);
    switch (src->type)
    {
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_SET:
    case YNODE_TYPE_IMAP:
    {
        ytree_iter *iter = ytree_first(src->map);
        for (; iter != NULL; iter = ytree_next(src->map, iter))
        {
            ynode *src_child = ytree_data(iter);
            n += ynode_get_with_origin(src_child, origin, log);
        }
        break;
    }
    case YNODE_TYPE_OMAP:
    {
        ymap_iter *iter = ymap_first(src->omap);
        for (; iter != NULL; iter = ymap_next(src->omap, iter))
        {
            ynode *src_child = ymap_data(iter);
            n += ynode_get_with_origin(src_child, origin, log);
        }
        break;
    }
    case YNODE_TYPE_LIST:
    {
        ylist_iter *iter;
        for (iter = ylist_first(src->list);
             !ylist_done(src->list, iter);
             iter = ylist_next(src->list, iter))
        {
            ynode *src_child = ylist_data(iter);
            n += ynode_get_with_origin(src_child, origin, log);
        }
        break;
    }
    case YNODE_TYPE_VAL:
        break;
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    return n;
}

// get the src nodes' data using the log (ynode_log).
// return the number of value nodes printed to the log (ynode_log).
int ynode_get(ynode *src, ynode_log *log)
{
    return ynode_get_with_origin(src, -1, log);
}

// create single ynode and attach to parent
// return created ynode
ynode *ynode_create(node_type type, const char *tag, const char *key, char *value, ynode *parent, ynode_log *log)
{
    ynode *cur, *new;
    new = ynode_new(type, tag, value, 0);
    cur = ynode_control(NULL, new, parent, key, NULL, log);
    ynode_free(new);
    return cur;
}

// create new ynodes to parent using src
// return created ynode top
ynode *ynode_create_copy(ynode *src, ynode *parent, const char *key, ynode_log *log)
{
    return ynode_control(NULL, src, parent, key, NULL, log);
}

// create new ynodes using path
// return the last created ynode.
ynode *ynode_create_path(char *path, ynode *parent, ynode_log *log)
{
    ylist *keylist;
    ynode *found = parent;
    ynode *new = NULL;
    char *key = NULL;
    char *val = NULL;
    int type;
    if (!path)
        return NULL;
    keylist = ynode_path_tokenize(path, &val);
    if (!keylist)
        return NULL;
    if (parent)
        new = ynode_new(parent->type, parent->tag, NULL, 0);
    else
        new = ynode_new(YNODE_TYPE_MAP, NULL, NULL, 0);
    if (!new)
        goto failed;

    key = ylist_pop_front(keylist);
    while (key)
    {
        ynode *node;
        found = ynode_find_child(found, key);
        if (found)
        {
            // check type
            if (ylist_empty(keylist))
            {
                type = val ? YNODE_TYPE_VAL : YNODE_TYPE_MAP;
                node = ynode_new(type, NULL, val, 0);
                ynode_attach(node, new, key);
            }
            else
            {
                if (found->type == YNODE_TYPE_VAL)
                    type = YNODE_TYPE_MAP;
                else
                    type = found->type;
                node = ynode_new(type, NULL, val, 0);
                ynode_attach(node, new, key);
            }
        }
        else
        {
            if (ylist_empty(keylist))
            {
                type = val ? YNODE_TYPE_VAL : YNODE_TYPE_MAP;
                node = ynode_new(type, NULL, val, 0);
                ynode_attach(node, new, key);
            }
            else
            {
                node = ynode_new(YNODE_TYPE_MAP, NULL, NULL, 0);
                ynode_attach(node, new, key);
            }
        }
        free(key);
        if (!node)
            goto failed;
        new = node;
        key = ylist_pop_front(keylist);
    }
    ylist_destroy_custom(keylist, free);
    if (parent)
    {
        new = ynode_top(new);
        ynode_merge(parent, new, log);
        ynode_free(new);
        if (val)
            free(val);
        return ynode_search(parent, path);
    }
    if (val)
        free(val);
    return new;
failed:
    ynode_free(ynode_top(new));
    ylist_destroy_custom(keylist, free);
    if (val)
        free(val);
    return NULL;
}

// copy src ynodes (including all sub ynodes)
ynode *ynode_copy(ynode *src)
{
    ynode *dest;
    if (!src)
        return NULL;
    dest = ynode_new(src->type, src->tag, src->value, src->origin);
    if (!dest)
        return NULL;

    yhook_copy(dest, src);
    dest->tag = src->tag ? ystrdup((char *)src->tag) : NULL;

    switch (src->type)
    {
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_SET:
    case YNODE_TYPE_IMAP:
    {
        ytree_iter *iter = ytree_first(src->map);
        for (; iter != NULL; iter = ytree_next(src->map, iter))
        {
            ynode *src_child = ytree_data(iter);
            ynode *dest_child = ynode_copy(src_child);
            if (!dest_child)
                goto _fail;
            ynode_attach(dest_child, dest, ynode_key(src_child));
        }
        break;
    }
    case YNODE_TYPE_OMAP:
    {
        ymap_iter *iter = ymap_first(src->omap);
        for (; iter != NULL; iter = ymap_next(src->omap, iter))
        {
            ynode *src_child = ymap_data(iter);
            ynode *dest_child = ynode_copy(src_child);
            if (!dest_child)
                goto _fail;
            ynode_attach(dest_child, dest, ynode_key(src_child));
        }
        break;
    }
    case YNODE_TYPE_LIST:
    {
        ylist_iter *iter;
        for (iter = ylist_first(src->list);
             !ylist_done(src->list, iter);
             iter = ylist_next(src->list, iter))
        {
            ynode *src_child = ylist_data(iter);
            ynode *dest_child = ynode_copy(src_child);
            if (!dest_child)
                goto _fail;
            ynode_attach(dest_child, dest, NULL);
        }
        break;
    }
    case YNODE_TYPE_VAL:
        // nothing to do
        break;
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    return dest;
_fail:
    if (dest)
        ynode_free(dest);
    return NULL;
}

// merge src ynode to dest.
// dest is modified by the operation.
// return modified dest.
ynode *ynode_merge(ynode *dest, ynode *src, ynode_log *log)
{
    ynode *parent;
    if (!dest)
    {
        dest = ynode_copy(src);
        if (log && log->fp)
        {
            ynode_dump_to_fp(log->fp, dest, 1, YDB_LEVEL_MAX);
        }
        return dest;
    }

    parent = dest->parent;
    parent = ynode_control(dest, src, parent, ynode_key(dest), NULL, log);
    return parent;
}

// merge src ynode to dest.
// dest and src is not modified.
// New ynode is returned.
ynode *ynode_merge_new(ynode *dest, ynode *src)
{
    char *buf = NULL;
    size_t buflen = 0;
    FILE *fp;
    ynode *clone = NULL;
    if (!dest && !src)
        return NULL;
    fp = open_memstream(&buf, &buflen);
    if (fp)
    {
        if (dest->type == YNODE_TYPE_VAL)
            fprintf(fp, "%s", dest->value);
        else
            ynode_printf_to_fp(fp, dest, 0, YNODE_LEVEL_MAX);
        if (src->type == YNODE_TYPE_VAL)
            fprintf(fp, "%s", src->value);
        else
            ynode_printf_to_fp(fp, src, 0, YNODE_LEVEL_MAX);
        fclose(fp);
        ynode_scanf_from_buf(buf, buflen, 0, &clone);
        if (buf)
            free(buf);
    }
    return clone;
}

// deleted cur ynode (including all sub ynodes).
void ynode_delete(ynode *cur, ynode_log *log)
{
    if (cur)
        ynode_control(cur, NULL, cur->parent, ynode_key(cur), NULL, log);
}

struct ynode_traverse_data
{
    ynode_callback cb;
    void *addition;
    unsigned int flags;
};

static ydb_res ynode_traverse_sub(ynode *cur, struct ynode_traverse_data *tdata);
static int ynode_traverse_map(void *key, void *data, void *addition)
{
    ynode *cur = data;
    struct ynode_traverse_data *tdata = addition;
    key = (void *)key;
    return ynode_traverse_sub(cur, tdata);
}

static int ynode_traverse_list(void *data, void *addition)
{
    ynode *cur = data;
    struct ynode_traverse_data *tdata = addition;
    return ynode_traverse_sub(cur, tdata);
}

static ydb_res ynode_traverse_sub(ynode *cur, struct ynode_traverse_data *tdata)
{
    ydb_res res = YDB_OK;
    if (!IS_SET(tdata->flags, YNODE_LEAF_FIRST))
    {
        if (IS_SET(tdata->flags, YNODE_LEAF_ONLY)) // no child
        {
            switch (cur->type)
            {
            case YNODE_TYPE_MAP:
            case YNODE_TYPE_SET:
            case YNODE_TYPE_IMAP:
                if (ytree_size(cur->map) <= 0)
                    res = tdata->cb(cur, tdata->addition);
                break;
            case YNODE_TYPE_OMAP:
                if (ymap_size(cur->omap) <= 0)
                    res = tdata->cb(cur, tdata->addition);
                break;
            case YNODE_TYPE_LIST:
                if (ylist_empty(cur->list))
                    res = tdata->cb(cur, tdata->addition);
                break;
            case YNODE_TYPE_VAL:
                res = tdata->cb(cur, tdata->addition);
                break;
            default:
                break;
            }
        }
        else if (cur->type == YNODE_TYPE_VAL)
        {
            res = tdata->cb(cur, tdata->addition);
        }
        else
        {
            if (!IS_SET(tdata->flags, YNODE_VAL_ONLY))
                res = tdata->cb(cur, tdata->addition);
        }
    }
    if (res)
        return res;

    switch (cur->type)
    {
    case YNODE_TYPE_MAP:
    case YNODE_TYPE_SET:
    case YNODE_TYPE_IMAP:
        res = ytree_traverse(cur->map, ynode_traverse_map, tdata);
        break;
    case YNODE_TYPE_OMAP:
        res = ymap_traverse_order(cur->omap, ynode_traverse_map, tdata);
        break;
    case YNODE_TYPE_LIST:
        res = ylist_traverse(cur->list, ynode_traverse_list, tdata);
        break;
    case YNODE_TYPE_VAL:
        res = YDB_OK;
        break;
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    if (res)
        return res;

    if (IS_SET(tdata->flags, YNODE_LEAF_FIRST))
    {
        if (IS_SET(tdata->flags, YNODE_LEAF_ONLY)) // no child
        {
            switch (cur->type)
            {
            case YNODE_TYPE_MAP:
            case YNODE_TYPE_SET:
            case YNODE_TYPE_IMAP:
                if (ytree_size(cur->map) <= 0)
                    res = tdata->cb(cur, tdata->addition);
                break;
            case YNODE_TYPE_OMAP:
                if (ymap_size(cur->omap) <= 0)
                    res = tdata->cb(cur, tdata->addition);
                break;
            case YNODE_TYPE_LIST:
                if (ylist_empty(cur->list))
                    res = tdata->cb(cur, tdata->addition);
                break;
            case YNODE_TYPE_VAL:
                res = tdata->cb(cur, tdata->addition);
                break;
            default:
                break;
            }
        }
        else if (cur->type == YNODE_TYPE_VAL)
        {
            res = tdata->cb(cur, tdata->addition);
        }
        else
        {
            if (!IS_SET(tdata->flags, YNODE_VAL_ONLY))
                res = tdata->cb(cur, tdata->addition);
        }
    }
    return res;
}

ydb_res ynode_traverse(ynode *cur, ynode_callback cb, void *addition, unsigned int flags)
{
    struct ynode_traverse_data tdata;
    if (!cur || !cb)
        return YDB_E_INVALID_ARGS;
    tdata.cb = cb;
    tdata.addition = addition;
    tdata.flags = flags;
    return ynode_traverse_sub(cur, &tdata);
}

// find the ref ynode in target ynode tree.
// if ignore_index set, the first entry of the list are selected.
ynode *ynode_lookup(ynode *target, ynode *ref, int ignore_index)
{
    ylist *parents;
    if (!target || !ref)
        return NULL;
    parents = ylist_create();
    if (!parents)
        return NULL;

    // insert that has a parent.
    while (ref->parent)
    {
        ylist_push_front(parents, ref);
        ref = ref->parent;
    };

    while (!ylist_empty(parents) && target)
    {
        ref = ylist_pop_front(parents);
        int index = ynode_index(ref);
        if (index >= 0)
        {
            char key[64];
            sprintf(key, "%d", ignore_index ? 0 : index);
            target = ynode_find_child(target, key);
        }
        else
        {
            target = ynode_find_child(target, ynode_key(ref));
        }
    }

    ylist_destroy(parents);
    return target;
}

// update or create ynode n using the input string
ydb_res ynode_write(ynode **n, const char *format, ...)
{
    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;
    if (!n)
        return YDB_E_INVALID_ARGS;
    fp = open_memstream(&buf, &buflen);
    if (fp)
    {
        ydb_res res;
        ynode *src = NULL;
        va_list args;
        va_start(args, format);
        vfprintf(fp, format, args);
        va_end(args);
        fclose(fp);
        res = ynode_scanf_from_buf(buf, buflen, (*n) ? (*n)->origin : 0, &src);
        if (buf)
            free(buf);
        if (res)
        {
            ynode_remove(src);
            return res;
        }
        if (!src)
            return YDB_OK;
        if (*n)
        {
            ynode *top = ynode_merge(*n, src, NULL);
            ynode_remove(src);
            if (!top)
                return YDB_E_MERGE_FAILED;
            *n = top;
        }
        else
        {
            *n = src;
        }
        return YDB_OK;
    }
    return YDB_E_MEM_ALLOC;
}

ydb_res ynode_erase_sub(ynode *cur, void *addition)
{
    ynode *n = (void *)addition;
    ynode *target = ynode_lookup(n, cur, 1);
    if (target)
        ynode_delete(target, NULL);
    return YDB_OK;
}

// delete sub ynodes using the input string
ydb_res ynode_erase(ynode **n, const char *format, ...)
{
    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;
    if (!n)
        return YDB_E_INVALID_ARGS;
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
        res = ynode_scanf_from_buf(buf, buflen, (*n) ? (*n)->origin : 0, &src);
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
        flags = YNODE_LEAF_FIRST | YNODE_VAL_ONLY;
        res = ynode_traverse(src, ynode_erase_sub, *n, flags);
        ynode_remove(src);
        return res;
    }
    return YDB_E_MEM_ALLOC;
}

struct ynode_read_data
{
    ynode *top;
    ylist *varlist;
    int vartotal;
    int varnum;
};

ydb_res ynode_traverse_to_read(ynode *cur, void *addition)
{
    struct ynode_read_data *data = addition;
    const char *value = ynode_value(cur);
    if (value && strncmp(value, "+", 1) == 0)
    {
        ynode *n = ynode_lookup(data->top, cur, 0);
        if (n)
        {
            int index = atoi(value);
            void *p = ylist_data(ylist_index(data->varlist, index));
            // printf("index=%d p=%p\n", index, p);
            if (YLOG_SEVERITY_DEBUG)
            {
                char buf[512];
                ynode_dump_to_buf(buf, sizeof(buf), n, 0, 0);
                ylog_debug("%s", buf);
                ynode_dump_to_buf(buf, sizeof(buf), cur, 0, 0);
                ylog_debug("%s", buf);
            }
#if 0
            sscanf(ynode_value(n), &(value[4]), p);
#else
            int len = strlen(value);
            if (value[len - 1] == 's')
                strcpy(p, ynode_value(n));
            else
                sscanf(ynode_value(n), &(value[4]), p);
#endif
            data->varnum++;
        }
        else
        {
            if (YLOG_SEVERITY_DEBUG)
            {
                char *path = ynode_path(cur, YNODE_LEVEL_MAX, NULL);
                ylog_debug("no data for (%s)\n", path);
                free(path);
            }
        }
    }
    return YDB_OK;
}

// read the date from ynode grapes as the scanf()
int ydb_retrieve(ynode *n, const char *format, ...)
{
    ydb_res res;
    struct ynode_read_data data;
    ynode *src = NULL;
    unsigned int flags;
    va_list ap;
    int ap_num = 0;
    if (!n)
        return -1;
    res = ynode_scan(NULL, (char *)format, strlen(format), 0, &src, &ap_num);
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
    data.top = n;
    va_start(ap, format);
    ylog_debug("ap_num = %d\n", ap_num);
    do
    {
        void *p = va_arg(ap, void *);
        ylist_push_back(data.varlist, p);
        ylog_debug("p=%p\n", p);
        ap_num--;
    } while (ap_num > 0);
    va_end(ap);
    flags = YNODE_LEAF_FIRST | YNODE_VAL_ONLY;
    res = ynode_traverse(src, ynode_traverse_to_read, &data, flags);
    ylist_destroy(data.varlist);
    if (res)
    {
        ynode_remove(src);
        return -1;
    }
    ynode_remove(src);
    return data.varnum;
}