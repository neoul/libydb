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

#include "yalloc.h"
#include "ytree.h"
#include "ylist.h"

#include "ydb.h"
#include "ynode.h"

#define YNODE_LEVEL_MAX YDB_LEVEL_MAX
extern ydb_log_func ydb_logger;
#define ynode_log(severity, format, ...)                                         \
    do                                                                           \
    {                                                                            \
        if (ydb_log_severity < (severity))                                       \
            break;                                                               \
        ydb_logger(severity, (__FUNCTION__), (__LINE__), format, ##__VA_ARGS__); \
    } while (0)

#define ynode_log_debug(format, ...) ynode_log(YDB_LOG_DEBUG, format, ##__VA_ARGS__)
#define ynode_log_inout() ynode_log(YDB_LOG_INOUT, "\n")
#define ynode_log_in() ynode_log(YDB_LOG_INOUT, "{{ ------\n")
#define ynode_log_out() ynode_log(YDB_LOG_INOUT, "}}\n")
#define ynode_log_info(format, ...) ynode_log(YDB_LOG_INFO, format, ##__VA_ARGS__)
#define ynode_log_warn(format, ...) ynode_log(YDB_LOG_WARN, format, ##__VA_ARGS__)
#define ynode_log_error(format, ...) ynode_log(YDB_LOG_ERR, format, ##__VA_ARGS__)
#define YNODE_LOGGING_DEBUG (ydb_log_severity >= YDB_LOG_DEBUG)
#define YNODE_LOGGING_INFO (ydb_log_severity >= YDB_LOG_INFO)

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
    };
    unsigned int flags;
    int user_num;
    void *user[];
};
typedef struct _yhook yhook;

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
    unsigned char type;
    unsigned short origin;
    struct _ynode *parent;
    struct _yhook *hook;
};

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
                if (str[slen] == ' ')
                    space++;
                else
                    spacectrl++;
            }
            else
                ctrl++;
        }
    }

    if (space == 0 && ctrl == 0 && spacectrl == 0)
        return NULL;
    else
    {
        int len = 0;
        char *newstr = malloc((slen + spacectrl + (ctrl * 3) + 4));
        if (!newstr)
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
            else if (isspace(str[slen]))
            {
                int n;
                char c;
                switch (str[slen])
                {
                case 0x09:
                    c = 't';
                    break;
                case 0x0A:
                    c = 'n';
                    break;
                case 0x0B:
                    c = 'v';
                    break;
                case 0x0C:
                    c = 'f';
                    break;
                case 0x0D:
                    c = 'r';
                    break;
                default:
                    c = ' ';
                    break;
                }
                n = sprintf(newstr + len, "\\%c", c);
                if (n <= 0)
                    break;
                len = len + n;
            }
            else
            {
                int n = sprintf(newstr + len, "\\x%02X", str[slen]);
                if (n <= 0)
                    break;
                len = len + n;
            }
        }
        newstr[len] = '"';
        len++;
        newstr[len] = 0;
        return newstr;
    }
}

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
    case YNODE_TYPE_DICT:
        ytree_destroy_custom(node->dict, (user_free)ynode_free);
        break;
    case YNODE_TYPE_LIST:
        ylist_destroy_custom(node->list, (user_free)ynode_free);
        break;
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    yhook_delete(node);
    free(node);
}

// create ynode
static ynode *ynode_new(unsigned char type, char *value)
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
static ynode *ynode_detach(ynode *node)
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

// insert the ynode to the parent, the key is used for dict type.
// return old ynode that was being attached to the parent.
static ynode *ynode_attach(ynode *node, ynode *parent, char *key)
{
    ynode *old = NULL;
    if (!node || !parent)
        return NULL;
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
    return old;
}

// register the hook func to the target ynode.
ydb_res yhook_register(ynode *node, unsigned int flags, yhook_func func, int user_num, void *user[])
{
    yhook *hook;
    if (!node || !func)
        return YDB_E_INVALID_ARGS;
    if (IS_SET(flags, YNODE_LEAF_ONLY))
        return YDB_E_INVALID_FLAGS;
    if (user_num > 5 || user_num < 0)
        return YDB_E_INVALID_ARGS;
    if (!user && user_num > 0)
        return YDB_E_INVALID_ARGS;
    if (node->hook)
        hook = node->hook;
    else
    {
        hook = malloc(sizeof(yhook) + sizeof(void *) * user_num);
        memset(hook, 0x0, sizeof(yhook) + sizeof(void *) * user_num);
    }
    if (!hook)
        return YDB_E_NO_ENTRY;

    hook->flags = 0x0;
    if (IS_SET(flags, YNODE_LEAF_FIRST))
        SET_FLAG(hook->flags, YNODE_LEAF_FIRST);
    else
        UNSET_FLAG(hook->flags, YNODE_LEAF_FIRST);
    if (IS_SET(flags, YNODE_VAL_ONLY))
        SET_FLAG(hook->flags, YNODE_VAL_ONLY);
    else
        UNSET_FLAG(hook->flags, YNODE_VAL_ONLY);

    hook->node = node;
    hook->func = func;
    hook->user_num = user_num;
    if (user_num > 0)
        memcpy(hook->user, user, sizeof(void *) * user_num);
    node->hook = hook;
    return YDB_OK;
}

// unregister the hook func from the target ynode.
// return user data registered with the hook.
void yhook_unregister(ynode *node)
{
    yhook_delete(node);
}

static void yhook_func_exec(yhook *hook, char op, ydb_iter *cur, ydb_iter *_new)
{
    assert(hook->func);
    ynode_log_debug("user_num %d\n", hook->user_num);
    switch (hook->user_num)
    {
    case 0:
        hook->func0(op, cur, _new);
        break;
    case 1:
        hook->func1(hook->user[0], op, cur, _new);
        break;
    case 2:
        hook->func2(hook->user[0], op, cur, _new, hook->user[1]);
        break;
    case 3:
        hook->func3(hook->user[0], op, cur, _new, hook->user[1], hook->user[2]);
        break;
    case 4:
        hook->func4(hook->user[0], op, cur, _new, hook->user[1], hook->user[2], hook->user[3]);
        break;
    case 5:
        hook->func5(hook->user[0], op, cur, _new, hook->user[1], hook->user[2], hook->user[3], hook->user[4]);
        break;
    default:
        break;
    }
}

static int yhook_pre_run_for_delete(ynode *cur);
static int yhook_pre_run_for_delete_dict(void *key, void *data, void *addition)
{
    ynode *cur = data;
    key = (void *)key;
    addition = (void *)addition;
    return yhook_pre_run_for_delete(cur);
}

static int yhook_pre_run_for_delete_list(void *data, void *addition)
{
    ynode *cur = data;
    addition = (void *)addition;
    return yhook_pre_run_for_delete(cur);
}

// call the pre / post hook for deleting cur ynode.
static int yhook_pre_run_for_delete(ynode *cur)
{
    ydb_res res;
    ynode *node = cur;
    if (!cur)
        return YDB_E_NO_ENTRY;
    while (node)
    {
        yhook *hook = node->hook;
        if (hook && !(IS_SET(hook->flags, YNODE_LEAF_FIRST)))
        {
            if (cur->type == YNODE_TYPE_VAL)
                yhook_func_exec(hook, YHOOK_OP_DELETE, cur, NULL);
            else if (!IS_SET(hook->flags, YNODE_VAL_ONLY))
                yhook_func_exec(hook, YHOOK_OP_DELETE, cur, NULL);
            break;
        }
        node = node->parent;
    }
    switch (cur->type)
    {
    case YNODE_TYPE_DICT:
        res = ytree_traverse(cur->dict, yhook_pre_run_for_delete_dict, NULL);
        break;
    case YNODE_TYPE_LIST:
        res = ylist_traverse(cur->list, yhook_pre_run_for_delete_list, NULL);
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
        if (hook && IS_SET(hook->flags, YNODE_LEAF_FIRST))
        {
            if (cur->type == YNODE_TYPE_VAL)
                yhook_func_exec(hook, YHOOK_OP_DELETE, cur, NULL);
            else if (!IS_SET(hook->flags, YNODE_VAL_ONLY))
                yhook_func_exec(hook, YHOOK_OP_DELETE, cur, NULL);
            break;
        }
        node = node->parent;
    }
    return res;
}

// return ynode is changed or not.
static void yhook_pre_run(char op, ynode *parent, ynode *cur, ynode *new)
{
    yhook *hook;
    if (cur)
    {
        hook = cur->hook;
        if (hook && !(IS_SET(hook->flags, YNODE_LEAF_FIRST)))
        {
            if (cur->type == YNODE_TYPE_VAL)
                yhook_func_exec(hook, op, cur, new);
            else if (new && new->type == YNODE_TYPE_VAL)
                yhook_func_exec(hook, op, cur, new);
            else if (!IS_SET(hook->flags, YNODE_VAL_ONLY))
                yhook_func_exec(hook, op, cur, new);
        }
    }
    while (parent)
    {
        hook = parent->hook;
        if (hook && !(IS_SET(hook->flags, YNODE_LEAF_FIRST)))
        {
            if (cur && cur->type == YNODE_TYPE_VAL)
                yhook_func_exec(hook, op, cur, new);
            else if (new && new->type == YNODE_TYPE_VAL)
                yhook_func_exec(hook, op, cur, new);
            else if (!IS_SET(hook->flags, YNODE_VAL_ONLY))
                yhook_func_exec(hook, op, cur, new);
            break;
        }
        parent = parent->parent;
    }
}

static void yhook_post_run(char op, ynode *parent, ynode *cur, ynode *new)
{
    yhook *hook;
    if (cur)
    {
        hook = cur->hook;
        if (hook && IS_SET(hook->flags, YNODE_LEAF_FIRST))
        {
            if (cur->type == YNODE_TYPE_VAL)
                yhook_func_exec(hook, op, cur, new);
            else if (new && new->type == YNODE_TYPE_VAL)
                yhook_func_exec(hook, op, cur, new);
            else if (!IS_SET(hook->flags, YNODE_VAL_ONLY))
                yhook_func_exec(hook, op, cur, new);
        }
    }
    while (parent)
    {
        hook = parent->hook;
        if (hook && IS_SET(hook->flags, YNODE_LEAF_FIRST))
        {
            if (cur && cur->type == YNODE_TYPE_VAL)
                yhook_func_exec(hook, op, cur, new);
            else if (new && new->type == YNODE_TYPE_VAL)
                yhook_func_exec(hook, op, cur, new);
            else if (!IS_SET(hook->flags, YNODE_VAL_ONLY))
                yhook_func_exec(hook, op, cur, new);
            break;
        }
        parent = parent->parent;
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
    yhook_delete(dest);
    if (!src->hook)
        return;
    hook = malloc(sizeof(yhook) + sizeof(void *) * src->hook->user_num);
    if (hook)
        memcpy(hook, src->hook, sizeof(yhook) + sizeof(void *) * src->hook->user_num);
    dest->hook = hook;
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
        assert(!YDB_E_DUMP_CB);
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
    if (IS_SET(node->flags, YNODE_FLAG_KEY))
    {
        char *key = ystr_convert(node->key);
        res = _ynode_record_print(record, "{key: %s,", key ? key : node->key);
        if (key)
            free(key);
    }
    else if (IS_SET(node->flags, YNODE_FLAG_ITER))
        res = _ynode_record_print(record, "{key: %p,", ylist_data(node->iter));
    else
        res = _ynode_record_print(record, "{key: none,");
    if (res)
        return res;

    // print flags
    switch (node->type)
    {
    case YNODE_TYPE_VAL:
    {
        char *value = ystr_convert(node->value);
        res = _ynode_record_print(record, " value: %s,", value ? value : node->value);
        if (value)
            free(value);
        break;
    }
    case YNODE_TYPE_DICT:
    {
        res = _ynode_record_print(record, " dict: num=%d,", ytree_size(node->dict));
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
    if (IS_SET(node->flags, YNODE_FLAG_KEY))
    {
        char *key = ystr_convert(node->key);
        res = _ynode_record_print(record, "%s:", key ? key : node->key);
        if (key)
            free(key);
    }
    else if (IS_SET(node->flags, YNODE_FLAG_ITER))
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
        char *value = ystr_convert(node->value);
        res = _ynode_record_print(record, "%s%s\n",
                                  only_val ? "" : " ",
                                  value ? value : node->value);
        if (value)
            free(value);
    }
    else
    {
        res = _ynode_record_print(record, "\n");
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
    case YNODE_TYPE_DICT:
        res = ytree_traverse(node->dict, _ynode_record_traverse_dict, record);
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

struct _ynode_log
{
    FILE *fp;
    char *buf;
    size_t buflen;
    ynode *top;
    ynode *latest;
    int indent;
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

static void ynode_log_update(struct _ynode_log *log, ynode *cur)
{
    if (log && log->latest)
    {
        if (cur == log->latest)
        {
            log->latest = cur->parent;
            log->indent--;
        }
    }
}

static void ynode_log_print(struct _ynode_log *log, ynode *cur)
{
    int size;
    ynode *n;
    ylist *nodes;
    if (!log)
        return;
    nodes = ylist_create();
    if (!nodes)
        return;
    n = cur;

    while (n)
    {
        if (log->top == n)
            break;
        if (log->latest == n)
            break;
        ylist_push_front(nodes, n);
        n = n->parent;
    };

    size = ylist_size(nodes);
    n = ylist_pop_front(nodes);
    ynode_log_debug("nodes size=%d\n", size);
    while (n)
    {
        int only_val = 0;
        // print nodes and cur to fp using log->indent
        fprintf(log->fp, "%.*s", log->indent, space);
        // print key
        if (IS_SET(n->flags, YNODE_FLAG_KEY))
        {
            char *key = ystr_convert(n->key);
            fprintf(log->fp, "%s:", key ? key : n->key);
            if (key)
                free(key);
        }
        else if (IS_SET(n->flags, YNODE_FLAG_ITER))
            fprintf(log->fp, "-");
        else
            only_val = 1;

        // print value
        if (n->type == YNODE_TYPE_VAL)
        {
            char *value = ystr_convert(n->value);
            fprintf(log->fp, "%s%s\n",
                    only_val ? "" : " ",
                    value ? value : n->value);
            if (value)
                free(value);
        }
        else
            fprintf(log->fp, "\n");
        log->indent++;
        n = ylist_pop_front(nodes);
    }

    // update latest print node
    log->latest = cur;
    ylist_destroy(nodes);
    return;
}

int ydb_log_err_yaml(yaml_parser_t *parser)
{
    /* Display a parser error message. */
    switch (parser->error)
    {
    case YAML_MEMORY_ERROR:
        ynode_log_error("mem err: not enough memory for parsing\n");
        break;

    case YAML_READER_ERROR:
        if (parser->problem_value != -1)
        {
            ynode_log_error("reader error: %s: #%X at %zd\n", parser->problem,
                            parser->problem_value, parser->problem_offset);
        }
        else
        {
            ynode_log_error("reader error: %s at %zu\n", parser->problem,
                            parser->problem_offset);
        }
        break;

    case YAML_SCANNER_ERROR:
        if (parser->context)
        {
            ynode_log_error("scanner error: %s at line %zu, column %zu\n",
                            parser->context,
                            parser->context_mark.line + 1, parser->context_mark.column + 1);
            ynode_log_error("%s at line %zu, column %zu\n",
                            parser->problem, parser->problem_mark.line + 1,
                            parser->problem_mark.column + 1);
        }
        else
        {
            ynode_log_error("scanner error: %s at line %zu, column %zu\n",
                            parser->problem, parser->problem_mark.line + 1,
                            parser->problem_mark.column + 1);
        }
        break;

    case YAML_PARSER_ERROR:
        if (parser->context)
        {
            ynode_log_error("parser error: %s at line %zu, column %zu\n",
                            parser->context,
                            parser->context_mark.line + 1, parser->context_mark.column + 1);
            ynode_log_error("%s at line %zu, column %zu\n",
                            parser->problem, parser->problem_mark.line + 1,
                            parser->problem_mark.column + 1);
        }
        else
        {
            ynode_log_error("parser error: %s at line %zu, column %zu\n",
                            parser->problem, parser->problem_mark.line + 1,
                            parser->problem_mark.column + 1);
        }
        break;

    case YAML_COMPOSER_ERROR:
        if (parser->context)
        {
            ynode_log_error("composer error: %s at line %zu, column %zu\n",
                            parser->context,
                            parser->context_mark.line + 1, parser->context_mark.column + 1);
            ynode_log_error("%s at line %zu, column %zu\n",
                            parser->problem, parser->problem_mark.line + 1,
                            parser->problem_mark.column + 1);
            ynode_log_error("\n");
        }
        else
        {
            ynode_log_error("composer error: %s at line %zu, column %zu\n",
                            parser->problem, parser->problem_mark.line + 1,
                            parser->problem_mark.column + 1);
        }
        break;

    default:
        /* Couldn't happen. */
        ynode_log_error("internal error\n");
        break;
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

#define YNODE_FAIL(cond, root_cause)                            \
    if (cond)                                                   \
    {                                                           \
        res = root_cause;                                       \
        ynode_log_error("'%s': %s\n", #cond, ydb_res_str[res]); \
        break;                                                  \
    }

#define CLEAR_YVALUE(v) \
    do                  \
    {                   \
        if (v)          \
            yfree(v);   \
        v = NULL;       \
    } while (0)

ydb_res ynode_scan(FILE *fp, char *buf, int buflen, int origin, ynode **n, int *queryform)
{
    ydb_res res = YDB_OK;
    int level = 0;
    ylist *node_stack;
    ynode *top = NULL;
    ynode *node = NULL;
    ynode *old = NULL;
    char *val = NULL;
    char *scalar = NULL;
    struct ynode_query_data qdata;
    yaml_token_type_t last_token = YAML_NO_TOKEN;
    yaml_parser_t parser;
    yaml_token_t token;
    bool token_save = false;
    int node_type;

    if ((!fp && !buf) || !n)
    {
        res = YDB_E_INVALID_ARGS;
        return res;
    }

    node_stack = ylist_create();
    if (!node_stack)
    {
        res = YDB_E_MEM;
        return res;
    }

    /* Initialize parser */
    if (!yaml_parser_initialize(&parser))
    {
        ydb_log_err_yaml(&parser);
        yaml_parser_delete(&parser);
        res = YDB_E_YAML_INIT;
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

    do
    {
        yaml_parser_scan(&parser, &token);
        if (!token.type)
        {
            ydb_log_err_yaml(&parser);
            res = YDB_E_YAML_EMPTY_TOKEN;
            break;
        }
        if (YNODE_LOGGING_DEBUG)
        {
            if (token.type == YAML_BLOCK_END_TOKEN ||
                token.type == YAML_FLOW_MAPPING_END_TOKEN ||
                token.type == YAML_FLOW_SEQUENCE_END_TOKEN)
                level--;
            // if (token.type != YAML_SCALAR_TOKEN)
            ynode_log_debug("%.*s%s\n", level * 2, space, yaml_token_str[token.type]);
            if (token.type == YAML_BLOCK_SEQUENCE_START_TOKEN ||
                token.type == YAML_BLOCK_MAPPING_START_TOKEN ||
                token.type == YAML_FLOW_SEQUENCE_START_TOKEN ||
                token.type == YAML_FLOW_MAPPING_START_TOKEN)
                level++;
        }
        token_save = true;

        switch (token.type)
        {
        case YAML_KEY_TOKEN:
        case YAML_VALUE_TOKEN:
            if (last_token == token.type || (val && token.type == YAML_KEY_TOKEN))
            {
                if (val)
                {
                    node = ylist_back(node_stack);
                    ynode_log_debug("%.*s%s %s ** insert **\n",
                                    level * 2, space, val ? val : "", val ? ":" : "");
                    node = ynode_new(YNODE_TYPE_VAL, NULL);
                    node->origin = origin;
                    old = ynode_attach(node, ylist_back(node_stack), val);
                    ynode_free(old);
                    CLEAR_YVALUE(val);
                    top = node;
                }
            }
            break;
        /* Block delimeters */
        case YAML_BLOCK_SEQUENCE_START_TOKEN:
        case YAML_FLOW_SEQUENCE_START_TOKEN:
        case YAML_BLOCK_MAPPING_START_TOKEN:
        case YAML_FLOW_MAPPING_START_TOKEN:
        {
            if (token.type == YAML_BLOCK_SEQUENCE_START_TOKEN ||
                token.type == YAML_FLOW_SEQUENCE_START_TOKEN)
                node_type = YNODE_TYPE_LIST;
            else
                node_type = YNODE_TYPE_DICT;
            if (ylist_empty(node_stack))
            {
                if (top)
                {
                    YNODE_FAIL(node_type != top->type, YDB_E_INVALID_YAML_TOP);
                    node = top;
                }
                else
                {
                    node = ynode_new(node_type, NULL);
                    node->origin = origin;
                }
            }
            else
            {
                node = ynode_new(node_type, NULL);
                node->origin = origin;
            }
            YNODE_FAIL(!node, YDB_E_MEM);
            old = ynode_attach(node, ylist_back(node_stack), val);
            ynode_free(old);
            ylist_push_back(node_stack, node);
            CLEAR_YVALUE(val);
            ynode_log_debug("node_stack %d (last entry=%p)\n", ylist_size(node_stack), ylist_back(node_stack));
            break;
        }
        case YAML_BLOCK_ENTRY_TOKEN: // -
        {
            node = ylist_back(node_stack);
            YNODE_FAIL(!node, YDB_E_INVALID_YAML_ENTRY);
            YNODE_FAIL(node->type != YNODE_TYPE_LIST, YDB_E_INVALID_YAML_ENTRY);
            break;
        }
        case YAML_FLOW_ENTRY_TOKEN: // ,
            break;
        case YAML_BLOCK_END_TOKEN:
        case YAML_FLOW_MAPPING_END_TOKEN:
        case YAML_FLOW_SEQUENCE_END_TOKEN:
            if (val)
            {
                ynode_log_debug("%.*s%s ** insert **\n", level * 2, space, val);
                node = ynode_new(YNODE_TYPE_VAL, NULL);
                node->origin = origin;
                old = ynode_attach(node, ylist_back(node_stack), val);
                ynode_free(old);
                CLEAR_YVALUE(val);
            }
            top = ylist_pop_back(node_stack);
            ynode_log_debug("node_stack %d (last entry=%p)\n", ylist_size(node_stack), ylist_back(node_stack));
            break;
        case YAML_SCALAR_TOKEN:
            token_save = false;
            switch (last_token)
            {
            case YAML_KEY_TOKEN:
                scalar = (char *)token.data.scalar.value;
                ynode_log_debug("%.*s%s (set as a key)\n", level * 2, space, scalar);
                val = ystrdup(scalar);
                break;
            case YAML_STREAM_START_TOKEN:
            case YAML_DOCUMENT_START_TOKEN:
            case YAML_VALUE_TOKEN:
                scalar = (char *)token.data.scalar.value;
                ynode_log_debug("%.*s%s %s %s ** insert **\n", level * 2, space, val ? val : "", val ? ":" : "", scalar);
                node = ynode_new(YNODE_TYPE_VAL, scalar);
                node->origin = origin;
                old = ynode_attach(node, ylist_back(node_stack), val);
                ynode_free(old);
                CLEAR_YVALUE(val);
                top = node;
                break;
            case YAML_BLOCK_SEQUENCE_START_TOKEN:
            case YAML_FLOW_SEQUENCE_START_TOKEN:
            case YAML_BLOCK_MAPPING_START_TOKEN:
            case YAML_FLOW_MAPPING_START_TOKEN:
            case YAML_FLOW_ENTRY_TOKEN:
            case YAML_BLOCK_ENTRY_TOKEN:
                scalar = (char *)token.data.scalar.value;
                node = ylist_back(node_stack);
                if (node->type == YNODE_TYPE_DICT)
                {
                    if (val)
                    {
                        ynode_log_debug("%.*s%s %s %s ** insert **\n",
                                        level * 2, space, val ? val : "", val ? ":" : "", scalar ? scalar : "");
                        node = ynode_new(YNODE_TYPE_VAL, scalar);
                        node->origin = origin;
                        old = ynode_attach(node, ylist_back(node_stack), val);
                    }
                    else
                    {
                        node = ynode_new(YNODE_TYPE_VAL, NULL);
                        node->origin = origin;
                        old = ynode_attach(node, ylist_back(node_stack), scalar);
                        ynode_log_debug("%.*s%s %s ** insert **\n",
                                        level * 2, space, scalar ? scalar : "", scalar ? ":" : "");
                    }
                }
                else
                {
                    ynode_log_debug("%.*s%s %s %s ** insert **\n",
                                    level * 2, space, val ? val : "", val ? ":" : "", scalar);
                    node = ynode_new(YNODE_TYPE_VAL, scalar);
                    node->origin = origin;
                    old = ynode_attach(node, ylist_back(node_stack), val);
                }
                ynode_free(old);
                CLEAR_YVALUE(val);
                top = node;
                break;
            default:
                break;
            }
            break;
        case YAML_DOCUMENT_START_TOKEN:
        case YAML_DOCUMENT_END_TOKEN:
        case YAML_STREAM_START_TOKEN:
        case YAML_STREAM_END_TOKEN:
            break;
        case YAML_TAG_TOKEN:
            ynode_log_debug("handle=%s suffix=%s\n", token.data.tag.handle, token.data.tag.suffix);
        /* Others */
        case YAML_VERSION_DIRECTIVE_TOKEN:
        case YAML_TAG_DIRECTIVE_TOKEN:
        case YAML_ANCHOR_TOKEN:
        case YAML_ALIAS_TOKEN:
            token_save = false;
        default:
            break;
        }
        if (token_save)
            last_token = token.type;
        if (res)
            break;

        if (token.type != YAML_STREAM_END_TOKEN)
            yaml_token_delete(&token);
    } while (token.type != YAML_STREAM_END_TOKEN);
    yaml_token_delete(&token);
    yaml_parser_delete(&parser);

    if (!ylist_empty(node_stack) && !res)
        res = YDB_E_INVALID_YAML_INPUT;

    if (res)
    {
        if (!top)
            top = ylist_front(node_stack);
        while (!ylist_empty(node_stack))
            ylist_pop_back(node_stack);
        ynode_free(ynode_top(top));
        top = NULL;
    }
    ylist_destroy(node_stack);
    if (val)
        yfree(val);
    *n = top;
    if (queryform)
        *queryform = qdata.num_of_query;
    return res;
}

ydb_res ynode_scanf_from_fp(FILE *fp, ynode **n)
{
    if (fp && n)
        return ynode_scan(fp, NULL, 0, fileno(fp), n, 0);
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

static ynode *ynode_find_child(ynode *node, char *key)
{
    if (node->type == YNODE_TYPE_DICT)
    {
        return ytree_search(node->dict, key);
    }
    else if (node->type == YNODE_TYPE_LIST)
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
             !ylist_done(iter);
             iter = ylist_next(iter))
        {
            if (index == count)
                return ylist_data(iter);
            count++;
        }
        return NULL;
    }
    else
        return NULL;
}

// lookup the ynode in the path
ynode *ynode_search(ynode *node, char *path)
{
    int i, j;
    int failcount;
    ynode *found;
    char token[512];

    if (!path || !node)
        return NULL;
    if (node->type == YNODE_TYPE_VAL)
        return NULL;
    i = 0;
    j = 0;
    failcount = 0;
    if (path[0] == '/') // ignore first '/'
        i = 1;
    for (; path[i]; i++)
    {
        if (path[i] == '/')
        {
            token[j] = 0;
            ynode_log_debug("token: %s\n", token);
            found = ynode_find_child(node, token);
            if (found)
            {
                failcount = 0;
                node = found;
                j = 0;
                continue;
            }
            else
            {
                if (failcount > 0)
                    return NULL;
                failcount++;
            }
        }
        else if (path[i] == '=')
        {
            char *next = strchr(&path[i], '/');
            if (!next)
            {
                token[j] = 0;
                break;
            }
        }
        token[j] = path[i];
        j++;
    }
    // lookup the last token of the path.
    if (j > 0)
    {
        token[j] = 0;
        ynode_log_debug("*token: %s\n", token);
        found = ynode_find_child(node, token);
        if (found)
            return found;
        else
            return NULL;
    }
    return node;
}

// return ynodes' type
int ynode_type(ynode *node)
{
    if (node)
        return node->type;
    return -1;
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
    case YNODE_TYPE_DICT:
        return (ytree_size(node->dict) <= 0) ? 1 : 0;
    case YNODE_TYPE_LIST:
        return ylist_empty(node->list);
    default:
        assert(!YDB_E_TYPE_ERR);
    }
}

// return ynodes' value if that is a leaf.
char *ynode_value(ynode *node)
{
    if (node && node->type == YNODE_TYPE_VAL)
        return node->value;
    return NULL;
}

// return ynodes' key if that has a hash key.
char *ynode_key(ynode *node)
{
    if (!node || !node->parent)
        return NULL;
    if (node->parent->type == YNODE_TYPE_DICT)
        return node->key;
    return NULL;
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
             !ylist_done(iter);
             iter = ylist_next(iter))
        {
            if (node->iter == iter)
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
    case YNODE_TYPE_DICT:
        return ytree_data(ytree_first(node->dict));
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
    case YNODE_TYPE_DICT:
    {
        ytree_iter *iter = ytree_find(node->parent->dict, node->key);
        iter = ytree_prev(node->parent->dict, iter);
        return ytree_data(iter);
    }
    case YNODE_TYPE_LIST:
        return ylist_data(ylist_prev(node->iter));
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
    case YNODE_TYPE_DICT:
    {
        ytree_iter *iter = ytree_find(node->parent->dict, node->key);
        iter = ytree_next(node->parent->dict, iter);
        return ytree_data(iter);
    }
    case YNODE_TYPE_LIST:
        return ylist_data(ylist_next(node->iter));
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
    case YNODE_TYPE_DICT:
        return ytree_data(ytree_first(node->parent->dict));
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
    case YNODE_TYPE_DICT:
        return ytree_data(ytree_last(node->parent->dict));
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
    if (node && level >= 0)
    {
        int len, curlen;
        len = ynode_path_fprintf(fp, node->parent, level - 1);
        if (IS_SET(node->flags, YNODE_FLAG_KEY))
        {
            char *key = ystr_convert(node->key);
            curlen = fprintf(fp, "/%s", key ? key : node->key);
            if (key)
                free(key);
            if (curlen <= 0)
                return len;
            return len + curlen;
        }
        else if (IS_SET(node->flags, YNODE_FLAG_ITER))
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
    n = node->parent;
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
    return NULL;
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
    {
        char *value = ystr_convert(node->value);
        fprintf(fp, "=%s", value ? value : node->value);
        if (value)
            free(value);
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
    return NULL;
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
        if (cur->type == new->type)
        {
            if (cur->type == YNODE_TYPE_VAL)
            {
                if (strcmp(cur->value, new->value) == 0)
                    return YHOOK_OP_NONE;
                return YHOOK_OP_REPLACE;
            }
            return YHOOK_OP_NONE;
        }
        return YHOOK_OP_REPLACE;
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

static ynode *ynode_control(ynode *cur, ynode *src, ynode *parent, char *key, ynode_log *log)
{
    ynode *new = NULL;
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
        case YNODE_TYPE_DICT:
            if (!cur && key)
                cur = ytree_search(parent->dict, key);
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
        new = ynode_new(src->type, src->value);
        if (!new)
            return NULL;
        new->origin = src->origin;
    }
    else if (op == YHOOK_OP_NONE)
        new = cur;

    if (YNODE_LOGGING_DEBUG)
    {
        char *path = ynode_path_and_val(parent, YNODE_LEVEL_MAX, NULL);
        switch (op)
        {
        case YHOOK_OP_CREATE:
            ynode_log_debug("create %s to %s\n", key ? key : "-", path ? path : "top");
            break;
        case YHOOK_OP_REPLACE:
            ynode_log_debug("replace %s in %s\n", key ? key : "-", path ? path : "top");
            break;
        case YHOOK_OP_DELETE:
            ynode_log_debug("delete %s from %s\n", key ? key : "-", path ? path : "top");
            break;
        default:
            break;
        }
        if (path)
            free(path);
    }

    switch (op)
    {
    case YHOOK_OP_CREATE:
        ynode_attach(new, parent, key);
        yhook_pre_run(op, parent, cur, new);
        ynode_log_print(log, new);
        break;
    case YHOOK_OP_REPLACE:
        ynode_attach(new, parent, key);
        yhook_pre_run(op, parent, cur, new);
        ynode_log_print(log, new);
        break;
    case YHOOK_OP_DELETE:
        yhook_pre_run_for_delete(cur);
        ynode_log_print(log, cur);
        break;
    case YHOOK_OP_NONE:
    default:
        break;
    }

    if (src)
    {
        switch (src->type)
        {
        case YNODE_TYPE_DICT:
        {
            ytree_iter *iter = ytree_first(src->dict);
            for (; iter != NULL; iter = ytree_next(src->dict, iter))
            {
                ynode *src_child = ytree_data(iter);
                ynode *cur_child = ynode_find_child(new, src_child->key);
                ynode *new_child = ynode_control(cur_child, src_child, new, src_child->key, log);
                if (!new_child)
                    ynode_log_error("unable to add child node (src_child->key: %s)\n");
            }
            break;
        }
        case YNODE_TYPE_LIST:
        {
            ylist_iter *iter;
            for (iter = ylist_first(src->list);
                 !ylist_done(iter);
                 iter = ylist_next(iter))
            {
                ynode *src_child = ylist_data(iter);
                ynode *new_child = ynode_control(NULL, src_child, new, NULL, log);
                if (!new_child)
                    ynode_log_error("unable to add child node (src_child->key: %s)\n");
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
        ynode_log_update(log, new);
        yhook_post_run(op, parent, cur, new);
        break;
    case YHOOK_OP_REPLACE:
        ynode_log_update(log, new);
        yhook_post_run(op, parent, cur, new);
        ynode_free(cur);
        break;
    case YHOOK_OP_DELETE:
        ynode_log_update(log, cur);
        ynode_detach(cur);
        ynode_free(cur);
        break;
    case YHOOK_OP_NONE:
    default:
        break;
    }
    return new;
}

// create single ynode and attach to parent
// return created ynode
ynode *ynode_create(unsigned char type, char *key, char *value, ynode *parent, ynode_log *log)
{
    ynode *cur, *new;
    new = ynode_new(type, value);
    cur = ynode_control(NULL, new, parent, key, log);
    ynode_free(new);
    return cur;
}

// create new ynodes to parent using src
// return created ynode top
ynode *ynode_create_copy(ynode *src, ynode *parent, char *key, ynode_log *log)
{
    return ynode_control(NULL, src, parent, key, log);
}

// create new ynodes using path
// return the last created ynode.
ynode *ynode_create_path(char *path, ynode *parent, ynode_log *log)
{
    int i, j;
    char *key = NULL;
    ynode *new = NULL;
    ynode *node = NULL;
    char token[512];
    if (!path)
        return NULL;
    i = 0;
    j = 0;
    if (parent)
        new = ynode_new(parent->type, NULL);
    else
        new = ynode_new(YNODE_TYPE_DICT, NULL);
    if (!new)
        return NULL;

    if (path[0] == '/') // ignore first '/'
        i = 1;
    for (; path[i]; i++)
    {
        if (path[i] == '/') // '/' is working as delimiter
        {
            token[j] = 0;
            ynode_log_debug("token: %s\n", token);
            node = ynode_new(YNODE_TYPE_DICT, NULL);
            ynode_attach(node, new, token);
            new = node;
            j = 0;
        }
        else if (path[i] == '=')
        {
            if (key) // '=' is represented twice.
                goto _fail;
            token[j] = 0;
            ynode_log_debug("token: %s\n", token);
            key = ystrdup(token);
            if (!key)
                goto _fail;
            j = 0;
        }
        else
        {
            token[j] = path[i];
            j++;
        }
    }
    // lookup the last token of the path.
    if (j > 0)
    {
        token[j] = 0;
        if (key)
        {
            ynode_log_debug("key: %s, token: %s\n", key, token);
            node = ynode_new(YNODE_TYPE_VAL, token);
            ynode_attach(node, new, key);
            yfree(key);
            key = NULL;
        }
        else
        {
            ynode_log_debug("token: %s\n", token);
            node = ynode_new(YNODE_TYPE_DICT, NULL);
            ynode_attach(node, new, token);
        }
        new = node;
    }
    if (parent)
    {
        new = ynode_top(new);
        ynode_merge(parent, new, log);
        ynode_free(new);
        if (key)
            yfree(key);
        return ynode_search(parent, path);
    }
    return new;
_fail:
    if (key)
        yfree(key);
    ynode_free(ynode_top(new));
    return NULL;
}

// copy src ynodes (including all sub ynodes)
ynode *ynode_copy(ynode *src)
{
    ynode *dest;
    if (!src)
        return NULL;
    dest = ynode_new(src->type, src->value);
    if (!dest)
        return NULL;

    // copy other fields
    dest->origin = src->origin;
    yhook_copy(dest, src);

    switch (src->type)
    {
    case YNODE_TYPE_DICT:
    {
        ytree_iter *iter = ytree_first(src->dict);
        for (; iter != NULL; iter = ytree_next(src->dict, iter))
        {
            ynode *src_child = ytree_data(iter);
            ynode *dest_child = ynode_copy(src_child);
            if (!dest_child)
                goto _fail;
            ynode_attach(dest_child, dest, src_child->key);
        }
        break;
    }
    case YNODE_TYPE_LIST:
    {
        ylist_iter *iter;
        for (iter = ylist_first(src->list);
             !ylist_done(iter);
             iter = ylist_next(iter))
        {
            ynode *src_child = ylist_data(iter);
            ynode *dest_child = ynode_copy(src_child);
            if (!dest_child)
                goto _fail;
            ynode_attach(dest_child, dest, src_child->key);
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
    parent = ynode_control(dest, src, parent, dest->key, log);
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
        ynode_control(cur, NULL, cur->parent, cur->key, log);
}

struct ynode_traverse_data
{
    ynode_callback cb;
    void *addition;
    unsigned int flags;
};

static ydb_res ynode_traverse_sub(ynode *cur, struct ynode_traverse_data *tdata);
static int ynode_traverse_dict(void *key, void *data, void *addition)
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
            case YNODE_TYPE_DICT:
                if (ytree_size(cur->dict) <= 0)
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
    case YNODE_TYPE_DICT:
        res = ytree_traverse(cur->dict, ynode_traverse_dict, tdata);
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
            case YNODE_TYPE_DICT:
                if (ytree_size(cur->dict) <= 0)
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

// find the ref ynode on the same position if val_search = false.
// find the ref ynode on the same key and value node if val_search = true.
ynode *ynode_lookup(ynode *target, ynode *ref, int val_search)
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

    if (val_search)
    {
        while (!ylist_empty(parents) && target)
        {
            ref = ylist_pop_front(parents);
            switch (target->type)
            {
            case YNODE_TYPE_DICT:
                if (IS_SET(ref->flags, YNODE_FLAG_KEY))
                    target = ynode_find_child(target, ref->key);
                else
                    target = NULL;
                break;
            case YNODE_TYPE_LIST:
                if (IS_SET(ref->flags, YNODE_FLAG_ITER))
                {
                    if (ref->type == YNODE_TYPE_VAL)
                    {
                        ynode *tar_child;
                        ylist_iter *iter = ylist_first(target->list);
                        for (; !ylist_done(iter); iter = ylist_next(iter))
                        {
                            tar_child = ylist_data(iter);
                            if (tar_child->type == YNODE_TYPE_VAL &&
                                strcmp(tar_child->value, ref->value) == 0)
                            {
                                target = tar_child;
                                break;
                            }
                        }
                    }
                    else
                        target = NULL;
                }
                else
                    target = NULL;
                break;
            case YNODE_TYPE_VAL:
                target = NULL;
                break;
            default:
                assert(!YDB_E_TYPE_ERR);
            }
        }
    }
    else
    {
        while (!ylist_empty(parents) && target)
        {
            ref = ylist_pop_front(parents);
            int index = ynode_index(ref);
            if (index >= 0)
            {
                char key[64];
                sprintf(key, "%d", index);
                target = ynode_find_child(target, key);
            }
            else
            {
                target = ynode_find_child(target, ref->key);
            }
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
    return YDB_E_MEM;
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
    return YDB_E_MEM;
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
    char *value = ynode_value(cur);
    if (value && strncmp(value, "+", 1) == 0)
    {
        ynode *n = ynode_lookup(data->top, cur, 0);
        if (n)
        {
            int index = atoi(value);
            void *p = ylist_data(ylist_index(data->varlist, index));
            // printf("index=%d p=%p\n", index, p);
            if (YNODE_LOGGING_DEBUG)
            {
                char buf[512];
                ynode_dump_to_buf(buf, sizeof(buf), n, 0, 0);
                ynode_log_debug("%s", buf);
                ynode_dump_to_buf(buf, sizeof(buf), cur, 0, 0);
                ynode_log_debug("%s", buf);
            }
            sscanf(ynode_value(n), &(value[4]), p);
            data->varnum++;
        }
        else
        {
            if (YNODE_LOGGING_DEBUG)
            {
                char *path = ynode_path(cur, YNODE_LEVEL_MAX, NULL);
                ynode_log_debug("no data for (%s)\n", path);
                free(path);
            }
        }
    }
    return YDB_OK;
}

// read the date from ynode grapes as the scanf()
int ynode_read(ynode *n, const char *format, ...)
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
    ynode_log_debug("ap_num = %d\n", ap_num);
    do
    {
        void *p = va_arg(ap, void *);
        ylist_push_back(data.varlist, p);
        ynode_log_debug("p=%p\n", p);
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