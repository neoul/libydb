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

#define YDB_VNAME(NAME) #NAME
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
        YDB_VNAME(YDB_E_INVALID_YAML),
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
static ydb_log_func ydb_logger = ydb_log_func_example;
int ydb_log_register(ydb_log_func func)
{
    ydb_logger = func;
    return 0;
}

#define ydb_log(severity, format, ...)                                               \
    do                                                                               \
    {                                                                                \
        if (ydb_log_severity < (severity))                                           \
            break;                                                                   \
        if (ydb_logger)                                                              \
            ydb_logger(severity, (__FUNCTION__), (__LINE__), format, ##__VA_ARGS__); \
    } while (0)

#define ydb_log_debug(format, ...) ydb_log(YDB_LOG_DBG, format, ##__VA_ARGS__)
#define ydb_log_info(format, ...) ydb_log(YDB_LOG_INFO, format, ##__VA_ARGS__)
#define ydb_log_error(format, ...) ydb_log(YDB_LOG_INFO, format, ##__VA_ARGS__)
#define ydb_log_res(res) ydb_log(YDB_LOG_ERR, "%s\n", ydb_err_str[res])

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

// delete ynode regardless of the detachment of the parent
void ynode_free(ynode *node)
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
#define DUMP_FLAG_DEBUG 0x1
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

int dump_ctrl_print(struct dump_ctrl *dump, const char *format, ...)
{
    va_list args;
    va_start(args, format);
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
    va_end(args);
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
    if (res)
        return res;

    res = dump_ctrl_print(dump, "%p ", node);
    if (res)
        return res;

    // print key
    if (IS_SET(node->flags, YNODE_FLAG_KEY))
    {
        char *key = ystr_convert(node->key);
        res = dump_ctrl_print(dump, "{key: %s,", key ? key : node->key);
        if (key)
            free(key);
    }
    else if (IS_SET(node->flags, YNODE_FLAG_ITER))
        res = dump_ctrl_print(dump, "{key: %p,", ylist_data(node->iter));
    else
        res = dump_ctrl_print(dump, "{key: none,");
    if (res)
        return res;

    // print flags
    switch (node->type)
    {
    case YNODE_TYPE_VAL:
    {
        char *value = ystr_convert(node->value);
        res = dump_ctrl_print(dump, " value: %s,", value ? value : node->value);
        if (value)
            free(value);
        break;
    }
    case YNODE_TYPE_DICT:
    {
        res = dump_ctrl_print(dump, " dict: num=%d,", ytree_size(node->dict));
        break;
    }
    case YNODE_TYPE_LIST:
    {
        res = dump_ctrl_print(dump, " list: %s,", ylist_empty(node->list) ? "empty" : "not-empty");
        break;
    }
    default:
        assert(!YDB_E_TYPE_ERR);
    }
    if (res)
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
    if (res)
        return res;

    // print key
    if (IS_SET(node->flags, YNODE_FLAG_KEY))
    {
        char *key = ystr_convert(node->key);
        res = dump_ctrl_print(dump, "%s:", key ? key : node->key);
        if (key)
            free(key);
    }
    else if (IS_SET(node->flags, YNODE_FLAG_ITER))
        res = dump_ctrl_print(dump, "-");
    if (res)
        return res;

    // print value
    if (node->type == YNODE_TYPE_VAL)
    {
        char *value = ystr_convert(node->value);
        res = dump_ctrl_print(dump, " %s\n", value ? value : node->value);
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

void ynode_dump_node(FILE *fp, int fd, char *buf, int buflen, ynode *node, int level)
{
    struct dump_ctrl *dump;
    if (!node)
        return;
    dump = dump_ctrl_new_debug(fp, fd, buf, buflen, level);
    if (!dump)
        return;
    dump_ctrl_print(dump, "\n[dump]\n");
    dump_ctrl_dump_ynode(dump, node);
    dump_ctrl_print(dump, "[dump (len=%d)]\n", dump->len);
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

int ydb_log_err_yaml(yaml_parser_t *parser)
{
    /* Display a parser error message. */
    switch (parser->error)
    {
    case YAML_MEMORY_ERROR:
        ydb_log_error("mem err: not enough memory for parsing\n");
        break;

    case YAML_READER_ERROR:
        if (parser->problem_value != -1)
        {
            ydb_log_error("reader error: %s: #%X at %zd\n", parser->problem,
                          parser->problem_value, parser->problem_offset);
        }
        else
        {
            ydb_log_error("reader error: %s at %zu\n", parser->problem,
                          parser->problem_offset);
        }
        break;

    case YAML_SCANNER_ERROR:
        if (parser->context)
        {
            ydb_log_error("scanner error: %s at line %zu, column %zu\n",
                          parser->context,
                          parser->context_mark.line + 1, parser->context_mark.column + 1);
            ydb_log_error("%s at line %zu, column %zu\n",
                          parser->problem, parser->problem_mark.line + 1,
                          parser->problem_mark.column + 1);
        }
        else
        {
            ydb_log_error("scanner error: %s at line %zu, column %zu\n",
                          parser->problem, parser->problem_mark.line + 1,
                          parser->problem_mark.column + 1);
        }
        break;

    case YAML_PARSER_ERROR:
        if (parser->context)
        {
            ydb_log_error("parser error: %s at line %zu, column %zu\n",
                          parser->context,
                          parser->context_mark.line + 1, parser->context_mark.column + 1);
            ydb_log_error("%s at line %zu, column %zu\n",
                          parser->problem, parser->problem_mark.line + 1,
                          parser->problem_mark.column + 1);
        }
        else
        {
            ydb_log_error("parser error: %s at line %zu, column %zu\n",
                          parser->problem, parser->problem_mark.line + 1,
                          parser->problem_mark.column + 1);
        }
        break;

    case YAML_COMPOSER_ERROR:
        if (parser->context)
        {
            ydb_log_error("composer error: %s at line %zu, column %zu\n",
                          parser->context,
                          parser->context_mark.line + 1, parser->context_mark.column + 1);
            ydb_log_error("%s at line %zu, column %zu\n",
                          parser->problem, parser->problem_mark.line + 1,
                          parser->problem_mark.column + 1);
            ydb_log_error("\n");
        }
        else
        {
            ydb_log_error("composer error: %s at line %zu, column %zu\n",
                          parser->problem, parser->problem_mark.line + 1,
                          parser->problem_mark.column + 1);
        }
        break;

    default:
        /* Couldn't happen. */
        ydb_log_error("internal error\n");
        break;
    }
    return 0;
}

char *yaml_token_str[] = {
    YDB_VNAME(YAML_NO_TOKEN),

    YDB_VNAME(YAML_STREAM_START_TOKEN),
    YDB_VNAME(YAML_STREAM_END_TOKEN),

    YDB_VNAME(YAML_VERSION_DIRECTIVE_TOKEN),
    YDB_VNAME(YAML_TAG_DIRECTIVE_TOKEN),
    YDB_VNAME(YAML_DOCUMENT_START_TOKEN),
    YDB_VNAME(YAML_DOCUMENT_END_TOKEN),

    YDB_VNAME(YAML_BLOCK_SEQUENCE_START_TOKEN),
    YDB_VNAME(YAML_BLOCK_MAPPING_START_TOKEN),
    YDB_VNAME(YAML_BLOCK_END_TOKEN),

    YDB_VNAME(YAML_FLOW_SEQUENCE_START_TOKEN),
    YDB_VNAME(YAML_FLOW_SEQUENCE_END_TOKEN),
    YDB_VNAME(YAML_FLOW_MAPPING_START_TOKEN),
    YDB_VNAME(YAML_FLOW_MAPPING_END_TOKEN),

    YDB_VNAME(YAML_BLOCK_ENTRY_TOKEN),
    YDB_VNAME(YAML_FLOW_ENTRY_TOKEN),
    YDB_VNAME(YAML_KEY_TOKEN),
    YDB_VNAME(YAML_VALUE_TOKEN),

    YDB_VNAME(YAML_ALIAS_TOKEN),
    YDB_VNAME(YAML_ANCHOR_TOKEN),
    YDB_VNAME(YAML_TAG_TOKEN),
    YDB_VNAME(YAML_SCALAR_TOKEN),
};

ynode *ynode_fscanf(FILE *fp)
{
    ydb_res res = YDB_OK;
    int level = 0;
    yaml_parser_t parser;
    yaml_token_t token; /* new variable */
    ylist *stack;
    ynode *top = NULL;
    ynode *node = NULL;
    ylist_iter *last = NULL;
    char *key = NULL;
    int iskey = 0;

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
        ydb_log(YDB_LOG_DBG, "%.*s%s\n", level * 2, space, yaml_token_str[token.type]);
        switch (token.type)
        {
        case YAML_KEY_TOKEN:
            iskey = 1;
            if (key)
                yfree(key);
            key = NULL;
            break;
        case YAML_VALUE_TOKEN:
            iskey = 0;
            break;
            /* Block delimeters */
        case YAML_BLOCK_SEQUENCE_START_TOKEN:
        case YAML_BLOCK_MAPPING_START_TOKEN:
        {
            int node_type;
            if (token.type == YAML_BLOCK_SEQUENCE_START_TOKEN)
                node_type = YNODE_TYPE_LIST;
            else
                node_type = YNODE_TYPE_DICT;

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

            if (node)
            {
                ynode_attach(node, ylist_data(last), key);
                last = ylist_push_back(stack, node);
            }
            level++;
            break;
        }
        case YAML_BLOCK_ENTRY_TOKEN:
            iskey = 0;
            node = ylist_data(last);
            if (!node || (node && node->type != YNODE_TYPE_LIST))
                res = YDB_E_INVALID_YAML_ENTRY;
            break;
        case YAML_BLOCK_END_TOKEN:
            level--;
            top = ylist_pop_back(stack);
            last = ylist_last(stack);
            break;
        case YAML_SCALAR_TOKEN:
            if (iskey)
            {
                char *scalar = (char *)token.data.scalar.value;
                ydb_log(YDB_LOG_DBG, "%.*s%s\n", level * 2, space, scalar);
                key = ystrdup(scalar);
            }
            else
            {
                char *scalar = (char *)token.data.scalar.value;
                ydb_log(YDB_LOG_DBG, "%.*s%s\n", level * 2, space, scalar);
                node = ylist_data(last);
                if (node && node->type == YNODE_TYPE_DICT && !key)
                {
                    res = YDB_E_INVALID_YAML_KEY;
                    break;
                }
                node = ynode_new(YNODE_TYPE_VAL, scalar);
                res = ynode_attach(node, ylist_data(last), key);
                if (res)
                    ynode_free(node);
                if (key)
                    yfree(key);
                key = NULL;
            }
            break;
        case YAML_DOCUMENT_START_TOKEN:
            break;
        case YAML_DOCUMENT_END_TOKEN:
            break;
        case YAML_STREAM_START_TOKEN:
            level = 0;
            break;
        case YAML_STREAM_END_TOKEN:
            level = 0;
            break;
        /* Others */
        case YAML_FLOW_SEQUENCE_START_TOKEN:
        case YAML_FLOW_SEQUENCE_END_TOKEN:
        case YAML_FLOW_MAPPING_START_TOKEN:
        case YAML_FLOW_MAPPING_END_TOKEN:
        case YAML_FLOW_ENTRY_TOKEN:
        case YAML_VERSION_DIRECTIVE_TOKEN:
        case YAML_TAG_DIRECTIVE_TOKEN:
        case YAML_TAG_TOKEN:
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

ynode *ynode_scanf()
{
    return ynode_fscanf(stdin);
}

ynode *ynode_read(int fd)
{
    int dup_fd = dup(fd);
    FILE *fp = fdopen(dup_fd, "r");
    ynode *node = ynode_fscanf(fp);
    if (fp)
        fclose(fp);
    return node;
}

ynode *ynode_sscanf(char *buf, int buflen)
{
    FILE *fp;
    ynode *node;
    if (!buf || buflen < 0)
        return NULL;
    fp = fmemopen(buf, buflen, "r");
    node = ynode_fscanf(fp);
    if (fp)
        fclose(fp);
    return node;
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
        int index = 0;
        if(strspn(key, "0123456789") != strlen(key))
            return NULL;
        index = atoi(key);
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
    char token[512];
    ynode *found;

    if (!path || !node)
        return NULL;
    if (node->type == YNODE_TYPE_VAL)
        return NULL;
    i = 0;
    j = 0;
    failcount = 0;
    if(path[0] == '/') // ignore first '/'
        i = 1;
    for (; path[i]; i++)
    {
        if (path[i] == '/')
        {
            token[j] = 0;
            printf("@@token: %s\n", token);
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
        token[j] = path[i];
        j++;
    }
    // lookup the last token of the path.
    if (j > 0)
    {
        token[j] = 0;
        printf("@@token: %s\n", token);
        found = ynode_find_child(node, token);
        if (found)
            return found;
        else
            return NULL;
    }
    return node;
}

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
