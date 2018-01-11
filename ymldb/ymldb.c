#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>

// getpid()
#include <sys/types.h>
#include <unistd.h>

#include <yaml.h>
#include <cprops/avl.h>
#include <cprops/linked_list.h>
#include <cprops/trie.h>

#include "ymldb.h"

typedef enum ymldb_type_e {
    YMLDB_LEAF,
    YMLDB_LEAFLIST,
    YMLDB_BRANCH
} ymldb_type_t;

struct ycallback
{
    ymldb_callback_fn usr_func;
    void *usr_data;
    struct ynode *ydb;
    struct ymldb_callback_data *meta_data;
    int resv : 29;
    int deleted : 1;
    int type : 2;
};

struct ynode
{
    char *key;
    int no_record:1;
    int level:31;
    ymldb_type_t type;
    union {
        cp_avltree *children;
        char *value;
    };
    struct ynode *parent;
    struct ycallback *callback;
};

struct ystream
{
    FILE *stream;
    size_t buflen;
    size_t len;
    int is_write:1;
    int is_dynamic:1;
    int options:30;
    char* buf;
};

// ymldb control block
#define YMLDB_SUBSCRIBER_MAX 8
struct ymldb_cb
{
    char *key;
    struct ynode *ydb;
    unsigned int flags;
    int fd_publisher;
    int fd_requester;
    int fd_subscriber[YMLDB_SUBSCRIBER_MAX];
    int fd_flags[YMLDB_SUBSCRIBER_MAX];
    int inprogress_cnt;
};

struct ymldb_distribution
{
    fd_set *set;
    int max;
    FILE *stream;
	int res;
};

struct ymldb_params
{
    struct ymldb_cb *cb;
    yaml_parser_t parser;
    yaml_document_t document;
    struct
    {
        unsigned int opcode;
        unsigned int sequence;
        FILE *stream;
    } in;
    struct
    {
        unsigned int opcode;
        unsigned int sequence;
        FILE *stream;
    } out;
    int res;
    struct ynode *last_ydb; // last updated ydb
    struct ystream *streambuffer;
    int resv : 28;
    int send_relay : 1;
    int request_and_reply : 1;
    int no_change : 1;
    int update_callback_en : 1;
};

static int g_ymldb_log = YMLDB_LOG_ERR;
static char *g_ymldb_logfile = NULL;
static int pid = 0;
#define _log_write(FP, ...) fprintf(FP, __VA_ARGS__);

static FILE *_log_open(FILE *stream)
{
    if (g_ymldb_logfile)
    {
        FILE *_log_stream = fopen(g_ymldb_logfile, "a");
        if (_log_stream)
            stream = _log_stream;
    }
    if(!pid) {
        pid = getpid();
    }
    return stream;
}

static void _log_close(FILE *stream)
{
    if (!stream)
        return;
    if (stream != stdout && stream != stderr)
        fclose(stream);
}

#define _log_entrance()                                        \
    do                                                         \
    {                                                          \
        if (g_ymldb_log < YMLDB_LOG_LOG)                       \
            break;                                             \
        FILE *_log_stream = _log_open(stdout);                 \
        if (!_log_stream)                                      \
            break;                                             \
        _log_write(_log_stream, "\n________________________________\nP%d:%s >>>\n", pid, __FUNCTION__); \
        _log_close(_log_stream);                               \
    } while (0)

#define _log_debug(...)                                                           \
    do                                                                            \
    {                                                                             \
        if (g_ymldb_log < YMLDB_LOG_LOG)                                          \
            break;                                                                \
        FILE *_log_stream = _log_open(stdout);                                    \
        if (!_log_stream)                                                         \
            break;                                                                \
        _log_write(_log_stream, "[ymldb:debug] P%d:%s:%d: ", pid, __FUNCTION__, __LINE__); \
        _log_write(_log_stream, __VA_ARGS__);                                     \
        _log_close(_log_stream);                                                  \
    } while (0)

#define _log_info(...)                                                           \
    do                                                                           \
    {                                                                            \
        if (g_ymldb_log < YMLDB_LOG_INFO)                                        \
            break;                                                               \
        FILE *_log_stream = _log_open(stdout);                                   \
        if (!_log_stream)                                                        \
            break;                                                               \
        _log_write(_log_stream, "[ymldb:info] P%d:%s:%d: ", pid, __FUNCTION__, __LINE__); \
        _log_write(_log_stream, __VA_ARGS__);                                    \
        _log_close(_log_stream);                                                 \
    } while (0)

#define _log_error(...)                                                 \
    do                                                                  \
    {                                                                   \
        if (g_ymldb_log < YMLDB_LOG_ERR)                                \
            break;                                                      \
        FILE *_log_stream = _log_open(stderr);                          \
        if (!_log_stream)                                               \
            break;                                                      \
        _log_write(_log_stream, "\n  [ymldb:error]\n\n");               \
        _log_write(_log_stream, "\tP%d:%s:%d\n\t", pid, __FUNCTION__, __LINE__); \
        _log_write(_log_stream, __VA_ARGS__);                           \
        _log_write(_log_stream, "\n");                                  \
        _log_close(_log_stream);                                        \
    } while (0)

#define _log_error_head()                                             \
    do                                                                \
    {                                                                 \
        if (g_ymldb_log < YMLDB_LOG_ERR)                              \
            break;                                                    \
        FILE *_log_stream = _log_open(stderr);                        \
        if (!_log_stream)                                             \
            break;                                                    \
        _log_write(_log_stream, "\n  [ymldb:error]\n\n");             \
        _log_write(_log_stream, "\tP%d:%s:%d\n", pid, __FUNCTION__, __LINE__); \
        _log_close(_log_stream);                                      \
    } while (0)

#define _log_error_body(...)                   \
    do                                         \
    {                                          \
        if (g_ymldb_log < YMLDB_LOG_ERR)       \
            break;                             \
        FILE *_log_stream = _log_open(stderr); \
        if (!_log_stream)                      \
            break;                             \
        _log_write(_log_stream, "\t");         \
        _log_write(_log_stream, __VA_ARGS__);  \
        _log_close(_log_stream);               \
    } while (0)

#define _log_error_tail()                      \
    do                                         \
    {                                          \
        if (g_ymldb_log < YMLDB_LOG_ERR)       \
            break;                             \
        FILE *_log_stream = _log_open(stderr); \
        if (!_log_stream)                      \
            break;                             \
        _log_write(_log_stream, "\n");         \
        _log_close(_log_stream);               \
    } while (0)

int _ymldb_log_error_parser(yaml_parser_t *parser)
{
    /* Display a parser error message. */
    switch (parser->error)
    {
    case YAML_MEMORY_ERROR:
        _log_error("not enough memory for parsing\n");
        break;

    case YAML_READER_ERROR:
        if (parser->problem_value != -1)
        {
            _log_error("reader error: %s: #%X at %zd\n", parser->problem,
                       parser->problem_value, parser->problem_offset);
        }
        else
        {
            _log_error("reader error: %s at %zu\n", parser->problem,
                       parser->problem_offset);
        }
        break;

    case YAML_SCANNER_ERROR:
        if (parser->context)
        {
            _log_error_head();
            _log_error_body("scanner error: %s at line %zu, column %zu\n",
                            parser->context,
                            parser->context_mark.line + 1, parser->context_mark.column + 1);
            _log_error_body("%s at line %zu, column %zu\n",
                            parser->problem, parser->problem_mark.line + 1,
                            parser->problem_mark.column + 1);
            _log_error_tail();
        }
        else
        {
            _log_error("scanner error: %s at line %zu, column %zu\n",
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        break;

    case YAML_PARSER_ERROR:
        if (parser->context)
        {
            _log_error_head();
            _log_error_body("parser error: %s at line %zu, column %zu\n",
                            parser->context,
                            parser->context_mark.line + 1, parser->context_mark.column + 1);
            _log_error_body("%s at line %zu, column %zu\n",
                            parser->problem, parser->problem_mark.line + 1,
                            parser->problem_mark.column + 1);
            _log_error_tail();
        }
        else
        {
            _log_error("parser error: %s at line %zu, column %zu\n",
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        break;

    case YAML_COMPOSER_ERROR:
        if (parser->context)
        {
            _log_error_head();
            _log_error_body("composer error: %s at line %zu, column %zu\n",
                            parser->context,
                            parser->context_mark.line + 1, parser->context_mark.column + 1);
            _log_error_body("%s at line %zu, column %zu\n",
                            parser->problem, parser->problem_mark.line + 1,
                            parser->problem_mark.column + 1);
            _log_error_tail();
        }
        else
        {
            _log_error("composer error: %s at line %zu, column %zu\n",
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        break;

    default:
        /* Couldn't happen. */
        _log_error("internal error\n");
        break;
    }
    return 0;
}

int ymldb_log_set(int log_level, char *log_file)
{
    static char _log_file[32];
    g_ymldb_log = log_level;
    if (log_file)
    {
        strcpy(_log_file, log_file);
        g_ymldb_logfile = _log_file;
    }
    else
    {
        g_ymldb_logfile = NULL;
    }
    return g_ymldb_log;
}

struct ystream *_ystream_alloc(size_t len);
void _ystream_close(struct ystream *buf);
FILE *_ystream_open(struct ystream *buf, char *rw);
void _ystream_free(struct ystream *buf);
struct ystream *_ystream_alloc_and_open(size_t len, char *rw);

static void _params_free(struct ymldb_params *params);
static struct ymldb_params *_params_alloc(struct ymldb_cb *cb, FILE *instream, FILE *outstream);
static int _params_document_load(struct ymldb_params *params);
static int _params_buf_init(struct ymldb_params *params);
static void _params_buf_dump(struct ymldb_params *params, struct ynode *ydb, int print_level, int no_print_children);
static int _params_buf_flush(struct ymldb_params *params, int forced);

static int _distribution_deinit(struct ymldb_cb *cb);
static int _distribution_init(struct ymldb_cb *cb, int flags);
static int _distribution_send(struct ymldb_params *params);

static struct ycallback *_callback_alloc(
    int type, ymldb_callback_fn usr_func, void *usr_data, struct ynode *ydb);
static void _callback_data_free(struct ymldb_callback_data *cd);
static void _callback_free(struct ycallback *callback);
static void _notify_callback_run(struct ynode *ydb, int deleted);
static void _notify_callback_run_pending();
static void _callback_unreg(struct ynode *ydb, int del);
static void _update_callback_run(struct ynode *ydb);

static cp_trie *g_key_pool = NULL;
static int g_alloc_count = 0;
static char g_empty_str[4] = {0, 0, 0, 0};

struct ymldb_key
{
    char *key; // ymldb key
    int ref;   // reference count;
};

void *_malloc(size_t s)
{
    if (!g_key_pool)
    {
        g_key_pool = cp_trie_create(COLLECTION_MODE_NOSYNC);
        _log_debug("g_key_pool is created...\n");
    }
    void *p = malloc(s);
    if (p)
    {
        // _log_debug("alloc p=%p\n", p);
        g_alloc_count++;
    }
    return p;
}

char *_strdup(char *src)
{
    struct ymldb_key *ykey;
    g_alloc_count++;
    if (!g_key_pool)
    {
        g_key_pool = cp_trie_create(COLLECTION_MODE_NOSYNC);
        _log_debug("g_key_pool is created...\n");
    }
    if (!src || src[0] == 0)
    {
        return g_empty_str;
    }
    ykey = cp_trie_exact_match(g_key_pool, src);
    if (ykey)
    {
        ykey->ref++;
    }
    else
    {
        ykey = malloc(sizeof(struct ymldb_key));
        if (!ykey)
            return NULL;
        ykey->key = strdup(src);
        if (!ykey->key)
        {
            free(ykey);
            return NULL;
        }
        ykey->ref = 1;
        int res = cp_trie_add(g_key_pool, ykey->key, ykey);
        if (res != 0)
        {
            free(ykey->key);
            free(ykey);
            return NULL;
        }
    }
    // _log_debug("alloc p=%p p->key=%p key=%s ref=%d \n", ykey, ykey->key, ykey->key, ykey->ref);
    return ykey->key;
}

void _free(void *p)
{
    g_alloc_count--;
    if (!p || p == g_empty_str)
    {
        return;
    }

    if (g_key_pool)
    {
        struct ymldb_key *ykey;
        ykey = cp_trie_exact_match(g_key_pool, (char *)p);
        if (ykey)
        {
            ykey->ref--;
            // _log_debug("free p=%p p->key=%p key=%s ref=%d \n", ykey, ykey->key, ykey->key, ykey->ref);
            if (ykey->ref <= 0)
            {
                cp_trie_remove(g_key_pool, ykey->key, NULL);
                free(ykey->key);
                free(ykey);
            }
        }
        else
        { // free malloc()
            // _log_debug("free p=%p\n", p);
            free(p);
        }
        if (cp_trie_count(g_key_pool) <= 0)
        {
            cp_trie_destroy(g_key_pool);
            g_key_pool = NULL;
            _log_debug("g_key_pool is freed...\n");
        }
        return;
    }
    else
    {
        // _log_debug("free p=%p\n", p);
        free(p);
    }
    return;
}

struct cp_trie_cb_data
{
    int node_count;
    int depth_total;
    int max_level;
};

void cp_trie_dump_node(cp_trie_node *node, int level, char *prefix, struct cp_trie_cb_data *cdata)
{
    int i;
    mtab_node *map_node;

    cdata->node_count++;
    cdata->depth_total += level;
    if (level > cdata->max_level)
        cdata->max_level = level;

    for (i = 0; i < node->others->size; i++)
    {
        map_node = node->others->table[i];
        while (map_node)
        {
            cp_trie_dump_node(map_node->value, level + 1, map_node->attr, cdata);
            map_node = map_node->next;
        }
    }

    for (i = 0; i < level; i++)
        printf("\t");
    struct ymldb_key *ykey = node->leaf;
    printf(" - %s => [%s][p=%p][key-p=%p]\n", prefix, ykey ? (char *)ykey->key : "", ykey, ykey ? ykey->key : NULL);
}

void key_pool_dump()
{
    cp_trie *grp = g_key_pool;
    struct cp_trie_cb_data cdata;
    cdata.node_count = 0;
    cdata.depth_total = 0;
    cdata.max_level = 0;

    cp_trie_dump_node(grp->root, 0, "", &cdata);

    printf("\n %d nodes, %d deepest, avg. depth is %.2f\n\n",
           cdata.node_count, cdata.max_level, (float)cdata.depth_total / cdata.node_count);
}

// #define _ENHANCED_
// #define free _free
// #define malloc _malloc
// #undef strdup
// #define strdup _strdup

#define S10 "          "
static char *g_space = S10 S10 S10 S10 S10 S10 S10 S10 S10 S10;

static struct ynode *g_ydb = NULL;
static cp_avltree *g_ycb = NULL;
static cp_avltree *g_fds = NULL;
static cp_list *g_callbacks = NULL;

static unsigned int g_sequence = 1;

static FILE *_ymldb_fopen_from_fd(int fd, char *rw)
{
    int dup_fd = dup(fd);
    return fdopen(dup_fd, rw);
}

static char *_str_dump(const char *src)
{
    static int dbgidx;
    static char dbgstr[4][512];
    char *str;
    int i = 0, j = 0;
    int quotation = 0;
    dbgidx = (dbgidx + 1) % 4;
    str = dbgstr[dbgidx];
    str++; // for "
    for (; src[i] > 0; i++)
    {
        if (src[i] == '\n')
        {
            str[j] = '\\';
            str[j + 1] = 'n';
            j = j + 2;
            quotation = 1;
        }
        else if (src[i] == '\t')
        {
            str[j] = '\\';
            str[j + 1] = 't';
            j = j + 2;
        }
        else
        {
            str[j] = src[i];
            j = j + 1;
        }
    }
    if (quotation)
    {
        str[j] = '\"';
        str[j + 1] = 0;
        str = str - 1;
        str[0] = '\"';
        return str;
    }
    str[j] = 0;
    return str;
}

static char *_ydb_type(ymldb_type_t type)
{
    if (type == YMLDB_BRANCH)
        return "branch";
    else if (type == YMLDB_LEAF)
        return "leaf";
    else if (type == YMLDB_LEAFLIST)
        return "leaflist";
    else
        return "unknown";
}

static char *_opcode_str(int opcode)
{
    static char opstr[64] = {0};
    // memset(opstr, 0, sizeof(opstr));
    opstr[0] = 0;
    strcat(opstr, "(");
    if (opcode & YMLDB_OP_SEQ)
        strcat(opstr, "seq|");
    if (opcode & YMLDB_OP_ACK)
        strcat(opstr, "ack|");
    if (opcode & YMLDB_OP_GET)
        strcat(opstr, "get|");
    if (opcode & YMLDB_OP_DELETE)
        strcat(opstr, "del|");
    if (opcode & YMLDB_OP_MERGE)
        strcat(opstr, "merge|");
    if (opcode & YMLDB_OP_SUBSCRIBER)
        strcat(opstr, "sub|");
    if (opcode & YMLDB_OP_PUBLISHER)
        strcat(opstr, "pub|");
    if (opcode & YMLDB_OP_SYNC)
        strcat(opstr, "sync|");
    strcat(opstr, ")");
    return opstr;
}

struct ymldb_cb *_ymldb_cb(char *major_key)
{
    if (major_key && g_ycb)
    {
        struct ymldb_cb *cb = cp_avltree_get(g_ycb, major_key);
        if (cb)
            return cb;
    }
    return NULL;
}

static cp_list *_ymldb_traverse_ancestors(struct ynode *ydb, int traverse_level)
{
    if (!ydb)
        return NULL;
    cp_list *templist = cp_list_create_nosync();
    ydb = ydb->parent;
    while (ydb && ydb->level >= traverse_level)
    {
        cp_list_insert(templist, ydb);
        ydb = ydb->parent;
    }

    return templist;
}

static void _ymldb_traverse_free(cp_list *templist)
{
    if (templist)
    {
        cp_list_destroy(templist);
    }
}

struct ynode *_ymldb_get_ancestor(struct ynode *ydb, int level)
{
    if (!ydb)
        return NULL;
    while (ydb && ydb->level > level)
    {
        ydb = ydb->parent;
    }
    if (ydb && level != ydb->level)
        return NULL;
    return ydb;
}

static void _ymldb_fprintf_node(FILE *stream, struct ynode *ydb, int print_level, int no_print_children);
static int _ymldb_fprintf_each_of_node(void *n, void *dummy)
{
    cp_avlnode *node = n;
    struct ynode *ydb = node->value;
    FILE *stream = dummy;
    _ymldb_fprintf_node(stream, ydb, ydb->level, 0); // not print parents
    return 0;
}

static void _ymldb_fprintf_node(FILE *stream, struct ynode *ydb, int print_level, int no_print_children)
{
    cp_list *ancestors;
    if (!ydb)
        return;
    if (print_level < ydb->level)
    { // print parents
        struct ynode *ancestor;
        cp_list_iterator iter;
        ancestors = _ymldb_traverse_ancestors(ydb, print_level);
        cp_list_iterator_init(&iter, ancestors, COLLECTION_LOCK_NONE);
        while ((ancestor = cp_list_iterator_next(&iter)))
        {
            if (ancestor->level == 0)
                continue;
            switch (ancestor->type)
            {
            case YMLDB_BRANCH:
                fprintf(stream, "%.*s%s:\n", (ancestor->level - 1) * 2, g_space, ancestor->key);
                break;
            case YMLDB_LEAFLIST:
                fprintf(stream, "%.*s- %s\n", (ancestor->level - 1) * 2, g_space, ancestor->key);
                break;
            case YMLDB_LEAF:
                fprintf(stream, "%.*s%s: %s\n", (ancestor->level - 1) * 2, g_space, ancestor->key, ancestor->value);
                break;
            }
        }
        _ymldb_traverse_free(ancestors);
    }

    if (ydb->type == YMLDB_BRANCH)
    {
        if (ydb->level != 0)
        { // not print out for top node
            fprintf(stream, "%.*s%s:\n", (ydb->level - 1) * 2, g_space, ydb->key);
        }
        if (no_print_children)
            return;
        cp_avltree_callback(ydb->children, _ymldb_fprintf_each_of_node, stream);
    }
    else if (ydb->type == YMLDB_LEAFLIST)
        fprintf(stream, "%.*s- %s\n", (ydb->level - 1) * 2, g_space, _str_dump(ydb->key));
    else
        fprintf(stream, "%.*s%s: %s\n", (ydb->level - 1) * 2, g_space, ydb->key, _str_dump(ydb->value));
    return;
}

void _ymldb_fprintf_head(FILE *stream, unsigned int opcode, unsigned int sequence)
{
    fprintf(stream, "# %u\n", sequence);

    // %TAG !merge! actusnetworks.com:op:
    if (opcode & YMLDB_OP_SEQ)
    {
        if (opcode & YMLDB_OP_SEQ_CON)
            fprintf(stream, "%s %s %s%u\n", "%TAG", YMLDB_TAG_OP_SEQ, YMLDB_TAG_SEQ_CON, sequence);
        else
            fprintf(stream, "%s %s %s%u\n", "%TAG", YMLDB_TAG_OP_SEQ, YMLDB_TAG_SEQ, sequence);
    }

    if (opcode & YMLDB_OP_ACK)
    {
        fprintf(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_ACK, YMLDB_TAG_ACK);
    }

    if (opcode & YMLDB_OP_MERGE)
    {
        fprintf(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_MERGE, YMLDB_TAG_MERGE);
    }
    else if (opcode & YMLDB_OP_DELETE)
    {
        fprintf(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_DELETE, YMLDB_TAG_DELETE);
    }
    else if (opcode & YMLDB_OP_GET)
    {
        fprintf(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_GET, YMLDB_TAG_GET);
    }
    else if (opcode & YMLDB_OP_SYNC)
    {
        fprintf(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_SYNC, YMLDB_TAG_SYNC);
    }

    if (opcode & YMLDB_OP_SUBSCRIBER)
    {
        fprintf(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_SUBSCRIBER, YMLDB_TAG_SUBSCRIBER);
    }
    else if (opcode & YMLDB_OP_PUBLISHER)
    {
        fprintf(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_PUBLISHER, YMLDB_TAG_PUBLISHER);
    }

    fprintf(stream, "---\n");
}

void _ymldb_fprintf_tail(FILE *stream)
{
    fprintf(stream, "\n...\n\n");
}

// support readonly or writeonly.
struct ystream *_ystream_alloc(size_t len)
{
    struct ystream *buf;
    buf = malloc(sizeof(struct ystream));
    if (buf)
    {
        buf->stream = NULL;
        if (len > 0)
        {
            buf->buf = malloc(len);
            if(!buf->buf)
            {
                free(buf);
                return NULL;
            }
            buf->buflen = len;
            buf->buf[0] = 0;
        }
        else
        {
            buf->buf = NULL;
            buf->buflen = 0;
        }
        buf->len = 0;
        buf->is_write = 0;
        buf->is_dynamic = 0;
    }
    return buf;
}

void _ystream_close(struct ystream *buf)
{
    if (buf)
    {
        if (buf->stream)
        {
            if (buf->is_write && !buf->is_dynamic)
            {
                buf->len = ftell(buf->stream);
                // fflush(buf->stream);
                buf->buf[buf->len] = 0;
            }
            else if (buf->is_dynamic)
            {
                buf->buflen = buf->len;
            }
            fclose(buf->stream);
            buf->stream = NULL;
        }
    }
    return;
}

FILE *_ystream_open(struct ystream *buf, char *rw)
{
    if (buf)
    {
        _ystream_close(buf);
        if (strncmp(rw, "r", 1) == 0)
        {
            buf->stream = fmemopen(buf->buf, buf->len, rw);
            buf->is_write = 0;
        }
        else
        { // w, w+
            if(buf->is_dynamic)
            {
                if(buf->buf)
                    free(buf->buf);
                buf->buf = NULL;
                buf->len = 0;
                buf->buflen = 0;
                buf->stream = open_memstream(&(buf->buf), &(buf->len));
                buf->is_write = 1;
                buf->is_dynamic = 1;
            }
            else if (buf->buflen > 0)
            {
                buf->len = 0;
                buf->buf[0] = 0;
                buf->stream = fmemopen(buf->buf, buf->buflen, rw);
                buf->is_write = 1;
                setbuf(buf->stream, NULL);
            }
        }
        if (!buf->stream)
            return NULL;
        return buf->stream;
    }
    return NULL;
}

void _ystream_free(struct ystream *buf)
{
    if (buf)
    {
        if (buf->stream)
            fclose(buf->stream);
        if(buf->buf)
            free(buf->buf);
        free(buf);
    }
}

struct ystream *_ystream_alloc_and_open(size_t len, char *rw)
{
    struct ystream *buf = _ystream_alloc(len);
    _ystream_open(buf, rw);
    if (buf && buf->stream)
        return buf;
    _ystream_free(buf);
    return NULL;
}

struct ystream *_ystream_open_dynamic(void)
{
    struct ystream *buf = _ystream_alloc(0);
    if(buf)
    {
        buf->stream = open_memstream(&(buf->buf), &(buf->len));
        if(buf->stream) {
            buf->is_write = 1;
            buf->is_dynamic = 1;
            return buf;
        }
        _ystream_free(buf);
    }
    return NULL;
}

// Remove all subtree and data
static void _ymldb_node_free(void *vdata)
{
    struct ynode *ydb = vdata;
    if (ydb)
    {
        if (ydb->type == YMLDB_BRANCH)
        {
            // fixed BUG - prevent to loop infinitely in children free phase.
            cp_avltree *children = ydb->children;
            ydb->children = NULL;
            cp_avltree_destroy_custom(children, NULL, _ymldb_node_free);
        }
        
        _notify_callback_run(ydb, 1);
        if (ydb->callback)
        {
            int deleted = ydb->callback->deleted ? 0 : 1;
            _callback_unreg(ydb, deleted);
        }
        if (ydb->type != YMLDB_BRANCH)
            if (ydb->value)
                free(ydb->value);
        free(ydb->key);
        free(ydb);
    }
}

int _ymldb_print_level(struct ynode *last_ydb, struct ynode *cur_ydb)
{
    int print_level = 0;
    struct ynode *ancestor1;
    struct ynode *ancestor2;
    if (!last_ydb || !cur_ydb)
        return print_level;
    int search_max_level = cur_ydb->level < last_ydb->level ? cur_ydb->level : last_ydb->level;
    // _log_debug("\nsearch_max_level %d cur_ydb->level %d last_ydb->level %d\n",
    //     search_max_level, cur_ydb->level, last_ydb->level);
    // _log_debug("cur_ydb=%s last_ydb=%s\n", cur_ydb->key, last_ydb->key);
    while (print_level <= search_max_level)
    {
        ancestor1 = _ymldb_get_ancestor(cur_ydb, print_level);
        ancestor2 = _ymldb_get_ancestor(last_ydb, print_level);
        if (!ancestor1 || !ancestor2)
            break;
// _log_debug("@@@@ ancestor1=%s, ancestor2=%s\n", ancestor1->key, ancestor2->key);
#ifdef _ENHANCED_
        if (ancestor1->key != ancestor2->key)
            break;
#else
        if (strcmp(ancestor1->key, ancestor2->key) != 0)
            break;
#endif
        print_level++;
    }
    return print_level;
}

void _ymldb_node_merge_reply(struct ymldb_params *params, struct ynode *ydb)
{
    int print_level = 0;
    if (!params || !ydb)
        return;
    params->no_change = 0;
    print_level = _ymldb_print_level(params->last_ydb, ydb);
    _params_buf_dump(params, ydb, print_level, 0);
    _notify_callback_run(ydb, 0);
    return;
}

struct ynode *_ymldb_node_merge(struct ymldb_params *params, struct ynode *parent,
                                ymldb_type_t type, char *key, char *value)
{
    // struct ycallback *callback = NULL;
    struct ynode *ydb = NULL;
    char *ykey = NULL;
    if (parent)
    {
        if (parent->type != YMLDB_BRANCH)
        {
            _log_error_head();
            _log_error_body("Unable to assign new ymldb to a leaf ymldb node.\n");
            _log_error_body(" - parent: %s, child %s\n", parent->key, key);
            return NULL;
        }

        ydb = cp_avltree_get(parent->children, key);
        if (ydb)
        {
            // check if the key exists.
            // notify the change if they differ.
            if (ydb->type != type)
            {
                _log_debug("different type (%s %s-->%s)\n",
                           ydb->key, _ydb_type(ydb->type), _ydb_type(type));
                if (ydb->type == YMLDB_BRANCH && ydb->children)
                    cp_avltree_destroy_custom(ydb->children, NULL, _ymldb_node_free);
                else if (ydb->value)
                    free(ydb->value);

                ydb->type = type;
                if (ydb->type == YMLDB_BRANCH)
                {
                    ydb->children = cp_avltree_create((cp_compare_fn)strcmp);
                    if (!ydb->children)
                        goto free_ydb;
                }
                else if (type == YMLDB_LEAFLIST)
                {
                    ydb->value = strdup(key);
                    if (!ydb->value)
                        goto free_ydb;
                }
                else
                {
                    ydb->value = strdup(value);
                    if (!ydb->value)
                        goto free_ydb;
                }
                _ymldb_node_merge_reply(params, ydb);
                // if (ydb->callback)
                // {
                //     callback = ydb->callback;
                //     ydb->callback = NULL;
                // }
                // cp_avltree_delete(parent->children, ydb->key);
                // _ymldb_node_free(ydb);
                // goto new_ydb;
            }
            else if (ydb->type == YMLDB_LEAF)
            {
                if (strcmp(ydb->value, value) != 0)
                {
                    free(ydb->value);
                    ydb->value = strdup(value);
                    _ymldb_node_merge_reply(params, ydb);
                }
            }
            return ydb;
        }
    }

    // new_ydb:
    ydb = malloc(sizeof(struct ynode));
    if (!ydb)
        goto free_ydb;
    memset(ydb, 0, sizeof(struct ynode));

    ykey = strdup(key);
    if (!ykey)
        goto free_ydb;

    ydb->key = ykey;
    ydb->type = type;

    // ydb->callback = callback;
    // if (callback)
    //     callback->ydb = ydb;

    if (type == YMLDB_BRANCH)
    {
        ydb->children = cp_avltree_create((cp_compare_fn)strcmp);
        if (!ydb->children)
            goto free_ydb;
    }
    else if (type == YMLDB_LEAFLIST)
    {
        ydb->value = strdup(key);
        if (!ydb->value)
            goto free_ydb;
    }
    else
    {
        ydb->value = strdup(value);
        if (!ydb->value)
            goto free_ydb;
    }

    if (parent)
    {
        ydb->parent = parent;
        ydb->level = parent->level + 1;
        cp_avltree_insert(parent->children, ydb->key, ydb);
    }
    else
    {
        ydb->parent = NULL;
        ydb->level = 0;
    }
    // _log_debug("ydb->key %s ydb->type %d ydb->value '%s'\n", ydb->key, ydb->type, ydb->value);
    // notify_ydb:
    _ymldb_node_merge_reply(params, ydb);
    return ydb;

free_ydb:
    _log_error("mem alloc failed for ymldb node.\n");
    if (ydb)
    {
        // if (callback) {
        //     _callback_free(callback);
        // }
        if (type == YMLDB_BRANCH && ydb->children)
        {
            cp_avltree_destroy_custom(ydb->children, NULL, _ymldb_node_free);
        }
        else
        {
            if (ydb->value)
                free(ydb->value);
        }
    }

    if (ykey)
        free(ykey);
    if (ydb)
        free(ydb);
    return NULL;
}

void _ymldb_node_delete(struct ymldb_params *params, struct ynode *parent, char *key)
{
    int print_level = 0;
    if (!params || !parent || !key)
        return;

    struct ynode *ydb = NULL;
    if (parent->type != YMLDB_BRANCH)
    {
        _log_error("\n"
                   "\tUnable to delete a node from value.\n"
                   "\tparent ymldb: %s, child ymldb %s\n",
                   parent->key, key);
        return;
    }

    ydb = cp_avltree_get(parent->children, key);
    if (ydb)
    {
        if (ydb->level <= 1)
        {
            _log_error("Unable to delete major ymldb branch.\n");
            params->res--;
            return;
        }
    }
    else
    {
        _log_error("'%s' doesn't exists\n", key);
        params->res--;
        return;
    }

    params->no_change = 0;
    print_level = _ymldb_print_level(params->last_ydb, ydb);
    _params_buf_dump(params, ydb, print_level, 1);
    // parent should be saved because of the ydb will be removed.
    params->last_ydb = parent;
    ydb = cp_avltree_delete(parent->children, key);
    _ymldb_node_free(ydb);
    return;
}

void _ymldb_node_get(struct ymldb_params *params, struct ynode *parent, char *key)
{
    int print_level = 0;
    struct ynode *ydb = NULL;
    if (!params || !parent || !key)
        return;

    if (parent->type != YMLDB_BRANCH)
    {
        _log_error("\n"
                   "\tUnable to get a node from value.\n"
                   "\tparent ymldb: %s, child ymldb %s\n",
                   parent->key, key);
        return;
    }
    if (key[0] > 0)
    {
        ydb = cp_avltree_get(parent->children, key);
        if (!ydb)
        {
            _log_error("'%s' doesn't exists\n", key);
            params->res--;
            return;
        }
        // ydb->callback will be checked on _params_buf_dump()
        if (parent->callback)
        {
            if (parent->callback->type == YMLDB_UPDATE_CALLBACK)
                _update_callback_run(parent);
        }
    }
    else
    {
        ydb = parent;
    }

    print_level = _ymldb_print_level(params->last_ydb, ydb);
    _params_buf_dump(params, ydb, print_level, 0);
    return;
}

int _ymldb_internal_merge(struct ymldb_params *params, struct ynode *p_ydb, int index, int p_index)
{
    struct ymldb_cb *cb = params->cb;
    yaml_node_t *node = NULL;
    if (!p_ydb)
    {
        _log_error("merge failed - unknown ydb\n");
        params->res--;
        return -1;
    }
    node = yaml_document_get_node(&params->document, index);
    if (!node)
        return 0;

    if (p_ydb->level == 1)
    {
        if (strcmp(cb->key, p_ydb->key) != 0)
        {
            _log_error("merge failed - key mismatch (%s, %s)\n", cb->key, p_ydb->key);
            params->res--;
            return -1;
        }
    }

    switch (node->type)
    {
    case YAML_SEQUENCE_NODE:
    {
        yaml_node_item_t *item;
        // printf("SEQ c=%d p=%d\n", index, p_index);
        for (item = node->data.sequence.items.start;
             item < node->data.sequence.items.top; item++)
        {
            yaml_node_t *node = yaml_document_get_node(&params->document, *item);
            char *key = (char *)node->data.scalar.value;
            _log_debug("## %s\n", key);
            if (node->type == YAML_SCALAR_NODE)
            {
                _ymldb_node_merge(params, p_ydb, YMLDB_LEAFLIST, key, NULL);
            }
            else
            {
                _ymldb_internal_merge(params, p_ydb, *item, index);
            }
        }
    }
    break;
    case YAML_MAPPING_NODE:
    {
        yaml_node_pair_t *pair;
        // printf("MAPPING c=%d p=%d\n", index, p_index);
        for (pair = node->data.mapping.pairs.start;
             pair < node->data.mapping.pairs.top; pair++)
        {
            yaml_node_t *key_node = yaml_document_get_node(&params->document, pair->key);
            yaml_node_t *value_node = yaml_document_get_node(&params->document, pair->value);
            char *key = (char *)key_node->data.scalar.value;
            char *value = (char *)value_node->data.scalar.value;

            if (value_node->type == YAML_SCALAR_NODE)
            {

                if (value[0] > 0)
                {
                    // if not empty string
                    _log_debug("## %s, %s\n", key, value);
                    _ymldb_node_merge(params, p_ydb, YMLDB_LEAF, key, value);
                }
                else
                {
                    // An empty string is created as a branch node
                    _log_debug("## %s\n", key);
                    _ymldb_node_merge(params, p_ydb, YMLDB_BRANCH, key, NULL);
                }
            }
            else
            { // not leaf
                struct ynode *ydb = NULL;
                _log_debug("## %s\n", key);
                if (p_ydb->level <= 0)
                    ydb = cp_avltree_get(p_ydb->children, key);
                else
                    ydb = _ymldb_node_merge(params, p_ydb, YMLDB_BRANCH, key, NULL);
                _ymldb_internal_merge(params, ydb, pair->value, index);
            }
        }
    }
    break;
    case YAML_SCALAR_NODE:
        break;
    case YAML_NO_NODE:
    default:
        break;
    }
    return 0;
}

int _ymldb_internal_delete(struct ymldb_params *params, struct ynode *p_ydb, int index, int p_index)
{
    struct ymldb_cb *cb = params->cb;
    yaml_node_t *node = NULL;
    if (!p_ydb)
    {
        _log_error("delete failed - unknown ydb\n");
        params->res--;
        return -1;
    }
    if (p_ydb->type != YMLDB_BRANCH)
    {
        _log_error("delete failed - parent is not branch node.\n");
        params->res--;
        return -1;
    }
    node = yaml_document_get_node(&params->document, index);
    if (!node)
        return 0;

    if (p_ydb->level == 1)
    {
        if (strcmp(cb->key, p_ydb->key) != 0)
        {
            _log_error("delete failed due to key mismatch (%s, %s)\n", cb->key, p_ydb->key);
            params->res--;
            return -1;
        }
    }

    switch (node->type)
    {
    case YAML_SEQUENCE_NODE:
    {
        yaml_node_item_t *item;
        // printf("SEQ c=%d p=%d\n", index, p_index);
        for (item = node->data.sequence.items.start;
             item < node->data.sequence.items.top; item++)
        {
            yaml_node_t *node = yaml_document_get_node(&params->document, *item);
            char *key = (char *)node->data.scalar.value;
            _log_debug("## %s\n", key);
            if (node->type == YAML_SCALAR_NODE)
            {
                _ymldb_node_delete(params, p_ydb, key);
            }
            else
            {
                _ymldb_internal_delete(params, p_ydb, *item, index);
            }
        }
    }
    break;
    case YAML_MAPPING_NODE:
    {
        yaml_node_pair_t *pair;
        // printf("MAPPING c=%d p=%d\n", index, p_index);
        for (pair = node->data.mapping.pairs.start;
             pair < node->data.mapping.pairs.top; pair++)
        {
            yaml_node_t *key_node = yaml_document_get_node(&params->document, pair->key);
            yaml_node_t *value_node = yaml_document_get_node(&params->document, pair->value);
            char *key = (char *)key_node->data.scalar.value;
            char *value = (char *)value_node->data.scalar.value;
            // _log_debug("key %s\n", key);
            if (value_node->type == YAML_SCALAR_NODE)
            {
                if (value[0] > 0)
                {
                    struct ynode *ydb = NULL;
                    _log_debug("## %s, %s\n", key, value);
                    ydb = cp_avltree_get(p_ydb->children, key);
                    _ymldb_node_delete(params, ydb, value);
                }
                else
                {
                    _log_debug("## %s\n", key);
                    _ymldb_node_delete(params, p_ydb, key);
                }
            }
            else
            { // not leaf
                struct ynode *ydb = NULL;
                _log_debug("## %s\n", key);
                ydb = cp_avltree_get(p_ydb->children, key);
                _ymldb_internal_delete(params, ydb, pair->value, index);
            }
        }
    }
    break;
    case YAML_SCALAR_NODE:
    { // It is only used for single key inserted..
        char *key;
        key = (char *)node->data.scalar.value;
        _log_debug("## %s\n", key);
        _ymldb_node_delete(params, p_ydb, key);
    }
    break;
    case YAML_NO_NODE:
    default:
        break;
    }
    return 0;
}

int _ymldb_internal_get(struct ymldb_params *params, struct ynode *p_ydb, int index, int p_index)
{
    // struct ymldb_cb *cb = params->cb;
    yaml_node_t *node = NULL;
    if (!p_ydb)
    {
        _log_error("get failed - unknown ydb\n");
        params->res--;
        return -1;
    }
    if (p_ydb->type != YMLDB_BRANCH)
    {
        _log_error("get failed - parent is not branch node.\n");
        params->res--;
        return -1;
    }
    node = yaml_document_get_node(&params->document, index);
    if (!node)
    {
        return 0;
    }
    params->update_callback_en = 1;

    switch (node->type)
    {
    case YAML_SEQUENCE_NODE:
    {
        yaml_node_item_t *item;
        // printf("SEQ c=%d p=%d\n", index, p_index);
        for (item = node->data.sequence.items.start;
             item < node->data.sequence.items.top; item++)
        {
            yaml_node_t *node = yaml_document_get_node(&params->document, *item);
            char *key = (char *)node->data.scalar.value;
            _log_debug("## %s\n", key);
            if (node->type == YAML_SCALAR_NODE)
            {
                _ymldb_node_get(params, p_ydb, key);
            }
            else
            {
                if (p_ydb->callback)
                {
                    if (p_ydb->callback->type == YMLDB_UPDATE_CALLBACK)
                        _update_callback_run(p_ydb);
                }
                _ymldb_internal_get(params, p_ydb, *item, index);
            }
        }
    }
    break;
    case YAML_MAPPING_NODE:
    {
        yaml_node_pair_t *pair;
        // printf("MAPPING c=%d p=%d\n", index, p_index);
        for (pair = node->data.mapping.pairs.start;
             pair < node->data.mapping.pairs.top; pair++)
        {
            yaml_node_t *key_node = yaml_document_get_node(&params->document, pair->key);
            yaml_node_t *value_node = yaml_document_get_node(&params->document, pair->value);
            char *key = (char *)key_node->data.scalar.value;
            char *value = (char *)value_node->data.scalar.value;
            if (value_node->type == YAML_SCALAR_NODE)
            {
                if (value[0] > 0)
                {
                    struct ynode *ydb = NULL;
                    _log_debug("## %s, %s\n", key, value);
                    ydb = cp_avltree_get(p_ydb->children, key);
                    _ymldb_node_get(params, ydb, value);
                }
                else
                {
                    _log_debug("## %s\n", key);
                    _ymldb_node_get(params, p_ydb, key);
                }
            }
            else
            { // not leaf
                struct ynode *ydb = NULL;
                _log_debug("## %s\n", key);
                if (p_ydb->callback)
                {
                    if (p_ydb->callback->type == YMLDB_UPDATE_CALLBACK)
                        _update_callback_run(p_ydb);
                }
                ydb = cp_avltree_get(p_ydb->children, key);
                _ymldb_internal_get(params, ydb, pair->value, index);
            }
        }
    }
    break;
    case YAML_SCALAR_NODE:
    { // It is only used for single key inserted..
        char *key;
        key = (char *)node->data.scalar.value;
        _log_debug("## %s\n", key);
        _ymldb_node_get(params, p_ydb, key);
    }
    break;
    case YAML_NO_NODE:
    default:
        break;
    }
    return 0;
}

int _ymldb_internal_relay(struct ymldb_params *params, int level, int index, int p_index)
{
    FILE *stream = params->streambuffer->stream;
    yaml_node_t *node = NULL;
    node = yaml_document_get_node(&params->document, index);
    if (!node)
    {
        return 0;
    }

    switch (node->type)
    {
    case YAML_SEQUENCE_NODE:
    {
        yaml_node_item_t *item;
        // printf("SEQ c=%d p=%d\n", index, p_index);
        for (item = node->data.sequence.items.start;
             item < node->data.sequence.items.top; item++)
        {
            yaml_node_t *node = yaml_document_get_node(&params->document, *item);
            char *key = (char *)node->data.scalar.value;
            _log_debug("## %s\n", key);
            if (node->type == YAML_SCALAR_NODE)
            {
                fprintf(stream, "%.*s- %s\n", level * 2, g_space, key);
            }
            else
            {
                _ymldb_internal_relay(params, level, *item, index);
            }
        }
    }
    break;
    case YAML_MAPPING_NODE:
    {
        yaml_node_pair_t *pair;
        // printf("MAPPING c=%d p=%d\n", index, p_index);
        for (pair = node->data.mapping.pairs.start;
             pair < node->data.mapping.pairs.top; pair++)
        {
            yaml_node_t *key_node = yaml_document_get_node(&params->document, pair->key);
            yaml_node_t *value_node = yaml_document_get_node(&params->document, pair->value);
            char *key = (char *)key_node->data.scalar.value;
            char *value = (char *)value_node->data.scalar.value;

            if (value_node->type == YAML_SCALAR_NODE)
            {
                _log_debug("## %s, %s\n", key, value);
                fprintf(stream, "%.*s%s: %s\n", level * 2, g_space, key, value);
            }
            else
            { // not leaf
                _log_debug("## %s\n", key);
                fprintf(stream, "%.*s%s:\n", level * 2, g_space, key);
                _ymldb_internal_relay(params, level + 1, pair->value, index);
            }
        }
    }
    break;
    case YAML_SCALAR_NODE:
    { // It is only used for single key inserted..
        char *key;
        key = (char *)node->data.scalar.value;
        _log_debug("## %s\n", key);
        fprintf(stream, "%.*s%s\n", level * 2, g_space, key);
    }
    break;
    case YAML_NO_NODE:
    default:
        break;
    }
    return 0;
}

int _params_opcode_extract(struct ymldb_params *params)
{
    unsigned int opcode = 0;
    unsigned int sequence = 0;
    yaml_document_t *document = &params->document;
    if (document->tag_directives.start != document->tag_directives.end)
    {
        char *op;
        yaml_tag_directive_t *tag;
        for (tag = document->tag_directives.start;
             tag != document->tag_directives.end; tag++)
        {
            op = (char *)tag->handle;
            if (strcmp(op, YMLDB_TAG_OP_SEQ) == 0)
            {
                char seq_type = 0;
                opcode = opcode | YMLDB_OP_SEQ;
                sscanf((char *)tag->prefix, YMLDB_TAG_SEQ_BASE "%c:%u",
                       &seq_type, &sequence);
                _log_debug("seq_type=%c\n", seq_type);
                if (seq_type == 'c')
                    opcode = opcode | YMLDB_OP_SEQ_CON;
            }
            else if (strcmp(op, YMLDB_TAG_OP_ACK) == 0)
            {
                opcode = opcode | YMLDB_OP_ACK;
            }
            else if (strcmp(op, YMLDB_TAG_OP_MERGE) == 0)
            {
                opcode = opcode | YMLDB_OP_MERGE;
            }
            else if (strcmp(op, YMLDB_TAG_OP_DELETE) == 0)
            {
                opcode = opcode | YMLDB_OP_DELETE;
            }
            else if (strcmp(op, YMLDB_TAG_OP_GET) == 0)
            {
                opcode = opcode | YMLDB_OP_GET;
            }
            else if (strcmp(op, YMLDB_TAG_OP_SUBSCRIBER) == 0)
            {
                opcode = opcode | YMLDB_OP_SUBSCRIBER;
            }
            else if (strcmp(op, YMLDB_TAG_OP_SYNC) == 0)
            {
                opcode = opcode | YMLDB_OP_SYNC;
            }
            else if (strcmp(op, YMLDB_TAG_OP_PUBLISHER) == 0)
            {
                opcode = opcode | YMLDB_OP_PUBLISHER;
            }
        }
    }
    params->in.opcode = opcode;
    params->in.sequence = sequence;
    return opcode;
}

enum internal_op
{
    iop_ignore,
    iop_get,
    iop_merge,
    iop_delete,
    iop_relay,
    iop_relay_delete,
};

static enum internal_op _ymldb_sm(struct ymldb_params *params)
{
    int ack = 0;
    int send_relay = 0;
    int request_and_reply = 0;
    int no_change = 1;
    unsigned int out_opcode = 0;
    unsigned int out_sequence = 0;
    unsigned int in_opcode = params->in.opcode;
    unsigned int in_sequence = params->in.sequence;
    unsigned int flags = params->cb->flags;
    enum internal_op iop = iop_ignore;
    _log_debug("in %uth\n", in_sequence);
    _log_debug("in %s\n", _opcode_str(in_opcode));
    if (in_opcode & YMLDB_OP_ACK)
        ack = 1;

    if (!(in_opcode & YMLDB_OP_ACTION))
    {
        // do nothing if no action.
        iop = iop_ignore;
        goto _done;
    }
    if (flags & YMLDB_FLAG_PUBLISHER)
    {
        out_opcode |= YMLDB_OP_PUBLISHER;
        if (in_opcode & YMLDB_OP_SUBSCRIBER)
        {
            if (in_opcode & (YMLDB_OP_MERGE | YMLDB_OP_DELETE))
                iop = iop_ignore;
            else // YMLDB_OP_GET, YMLDB_OP_SYNC
            {
                iop = iop_get;
                out_opcode |= (in_opcode & YMLDB_OP_SYNC) ? YMLDB_OP_SYNC : YMLDB_OP_GET;
                out_opcode |= YMLDB_OP_ACK;
                request_and_reply = 1;
            }
        }
        else if (in_opcode & YMLDB_OP_PUBLISHER)
        {
            if (ack)
            {
                if (in_opcode & YMLDB_OP_SYNC)
                    iop = iop_merge;
                else
                    iop = iop_ignore;
            }
            else
            {
                if (in_opcode & (YMLDB_OP_MERGE | YMLDB_OP_DELETE))
                {
                    iop = (in_opcode & YMLDB_OP_MERGE) ? iop_merge : iop_delete;
                    out_opcode |= (in_opcode & YMLDB_OP_MERGE) ? YMLDB_OP_MERGE : YMLDB_OP_DELETE;
                    if (!(flags & YMLDB_FLAG_ASYNC)) // sync mode
                        send_relay = 1;
                }
                else // (YMLDB_OP_GET | YMLDB_OP_SYNC)
                {
                    iop = iop_get;
                    out_opcode |= (in_opcode & YMLDB_OP_SYNC) ? YMLDB_OP_SYNC : YMLDB_OP_GET;
                    out_opcode |= YMLDB_OP_ACK;
                    request_and_reply = 1;
                }
            }
        }
        else
        {
            if (ack)
            {
                if (in_opcode & YMLDB_OP_SYNC)
                {
                    iop = iop_get;
                    out_opcode |= YMLDB_OP_SYNC;
                    out_opcode |= YMLDB_OP_ACK;
                    send_relay = 1;
                }
                else
                    iop = iop_ignore;
            }
            else
            {
                iop =
                    (in_opcode & YMLDB_OP_MERGE) ? iop_merge : 
                    (in_opcode & YMLDB_OP_DELETE) ? iop_delete : 
                    (in_opcode & YMLDB_OP_GET) ? iop_get :
                    (in_opcode & YMLDB_OP_SYNC) ? iop_relay : iop_ignore;
                    
                out_opcode |=
                    (in_opcode & YMLDB_OP_MERGE) ? YMLDB_OP_MERGE : 
                    (in_opcode & YMLDB_OP_DELETE) ? YMLDB_OP_DELETE : 
                    (in_opcode & YMLDB_OP_SYNC) ? YMLDB_OP_SYNC : 0;
                if(in_opcode & YMLDB_OP_SYNC)
                {
                    send_relay = 1;
                    if (!(flags & YMLDB_FLAG_SUB_PUBLISHER))
                        iop = iop_ignore;
                }
                else if (in_opcode & (YMLDB_OP_MERGE | YMLDB_OP_DELETE))
                {
                    _log_debug(" \n");
                    if (!(flags & YMLDB_FLAG_ASYNC)) // sync mode
                        send_relay = 1;
                }
            }
        }
    }
    else if (flags & YMLDB_FLAG_SUBSCRIBER)
    {
        out_opcode |= YMLDB_OP_SUBSCRIBER;
        if (in_opcode & YMLDB_OP_SUBSCRIBER)
            iop = iop_ignore;
        else if (in_opcode & YMLDB_OP_PUBLISHER)
        {
            if (ack && (in_opcode & YMLDB_OP_SYNC))
                iop = iop_merge;
            else if (!ack && (in_opcode & YMLDB_OP_MERGE))
                iop = iop_merge;
            else if (!ack && (in_opcode & YMLDB_OP_DELETE))
                iop = iop_delete;
            else
                iop = iop_ignore;
        }
        else
        {
            if (in_opcode & (YMLDB_OP_MERGE | YMLDB_OP_DELETE))
                iop = iop_ignore;
            else if (in_opcode & YMLDB_OP_SYNC)
            {
                // delete local ydb and sync relay
                iop = iop_relay_delete;
                out_opcode |= YMLDB_OP_SYNC;
                request_and_reply = 1;
            }
            else if (in_opcode & YMLDB_OP_GET)
            {
                if (flags & YMLDB_FLAG_ASYNC)
                {
                    iop = iop_relay;
                    out_opcode |= YMLDB_OP_SYNC;
                    request_and_reply = 1;
                }
                else
                {
                    iop = iop_get;
                }
            }
        }
    }
    else
    { // ymldb for local user
        if (in_opcode & YMLDB_OP_SUBSCRIBER)
            iop = iop_ignore;
        else if (in_opcode & YMLDB_OP_PUBLISHER)
            iop = iop_ignore;
        else
        {
            iop =
                (in_opcode & YMLDB_OP_MERGE) ? iop_merge : (in_opcode & YMLDB_OP_DELETE) ? iop_delete : (in_opcode & YMLDB_OP_SYNC) ? iop_ignore : iop_get;
            out_opcode = (in_opcode & YMLDB_OP_MERGE) ? YMLDB_OP_MERGE : (in_opcode & YMLDB_OP_DELETE) ? YMLDB_OP_DELETE : (in_opcode & YMLDB_OP_GET) ? YMLDB_OP_GET : 0;
        }
    }
_done:
    out_opcode |= YMLDB_OP_SEQ;
    if (in_opcode & YMLDB_OP_SEQ_CON)
        out_opcode |= YMLDB_OP_SEQ_CON;
    if (in_opcode & YMLDB_OP_SEQ)
    {
        out_sequence = in_sequence;
    }
    else
    {
        out_sequence = g_sequence;
        g_sequence++;
    }
    if (ack && request_and_reply == 1)
    {
        _log_debug("request/reply disabled due to ack!\n");
        request_and_reply = 0;
    }
    
    params->out.opcode = out_opcode;
    params->out.sequence = out_sequence;
    params->request_and_reply = request_and_reply;
    params->send_relay = send_relay;
    params->no_change = no_change;

    _log_debug("out %uth\n", out_sequence);
    _log_debug("out %s\n", _opcode_str(out_opcode));
    _log_debug("%s %s\n", 
        request_and_reply ? "request/reply" : "no-request/reply", 
        send_relay ? "send_relay" : "no-send_relay");
    _log_debug("internal op: %s\n",
               (iop == iop_ignore) ? "ignore" : 
               (iop == iop_merge) ? "merge" : 
               (iop == iop_delete) ? "delete" : 
               (iop == iop_relay) ? "relay" : 
               (iop == iop_get) ? "get" : 
               (iop == iop_relay_delete) ? "relay_and_delete" : "-");

    return iop;
}

static void _params_free(struct ymldb_params *params)
{
    yaml_parser_t *parser;
    yaml_document_t *document;
    struct ystream *streambuffer;
    if (!params)
        return;
    parser = &(params->parser);
    document = &(params->document);
    streambuffer = params->streambuffer;

    if (document->nodes.start)
    {
        yaml_document_delete(document);
    }
    if (parser->raw_buffer.start || parser->error)
    {
        yaml_parser_delete(parser);
    }
    if (streambuffer)
    {
        _ystream_free(streambuffer);
    }
    free(params);
    return;
}

static struct ymldb_params *_params_alloc(struct ymldb_cb *cb, FILE *instream, FILE *outstream)
{
    struct ymldb_params *params;
    yaml_parser_t *parser;

    params = malloc(sizeof(struct ymldb_params));
    if (!params)
    {
        _log_error("mem alloc failed.\n");
        return NULL;
    }
    memset(params, 0, sizeof(struct ymldb_params));

    parser = &(params->parser);
    if (!yaml_parser_initialize(parser))
    {
        _ymldb_log_error_parser(parser);
        goto failed;
    }
    yaml_parser_set_input_file(parser, instream);
    params->in.stream = instream;
    params->out.stream = outstream;
    params->in.opcode = 0;
    params->out.opcode = 0;
    params->in.sequence = 0;
    params->out.sequence = 0;
    params->streambuffer = _ystream_alloc(YMLDB_STREAM_BUF_SIZE);
    // _ystream_alloc_and_open(YMLDB_STREAM_BUF_SIZE, "w");
    if (!params->streambuffer)
    {
        _log_error("streambuffer alloc failed.\n");
        goto failed;
    }
    params->res = 0;
    params->cb = cb;
    return params;
failed:
    _ystream_free(params->streambuffer);
    _params_free(params);
    return NULL;
}

static int _params_document_load(struct ymldb_params *params)
{
    yaml_parser_t *parser = &params->parser;
    yaml_document_t *document = &params->document;
    if (document->nodes.start)
    {
        yaml_document_delete(document);
    }
    if (!yaml_parser_load(parser, document))
    {
        goto failed;
    }
    return 0;
failed:
    _ymldb_log_error_parser(parser);
    return -1;
}

static int _params_buf_init(struct ymldb_params *params)
{
    params->last_ydb = NULL;
    _ystream_open(params->streambuffer, "w");
    _ymldb_fprintf_head(params->streambuffer->stream, params->out.opcode, params->out.sequence);
    return 0;
}

static int _ymldb_param_streambuffer_each_of_node(void *n, void *dummy)
{
    cp_avlnode *node = n;
    struct ynode *ydb = node->value;
    struct ymldb_params *params = dummy;
    _params_buf_dump(params, ydb, ydb->level, 0); // not print parents
    return 0;
}

static void _params_buf_dump(struct ymldb_params *params, struct ynode *ydb, int print_level, int no_print_children)
{
    int flushed = 0;
    FILE *stream;
    cp_list *ancestors;
    if (!ydb)
        return;
    flushed = _params_buf_flush(params, 0);
    if (flushed)
    {
        _params_buf_init(params);
        _log_debug("print_level %d\n", print_level);
        _log_debug("cur ydb->key %s ydb->level %d\n", ydb->key, ydb->level);
        // no_print_children = 0;
        print_level = 0;
    }
    stream = params->streambuffer->stream;

    if (print_level < ydb->level)
    { // print parents
        struct ynode *ancestor;
        cp_list_iterator iter;
        ancestors = _ymldb_traverse_ancestors(ydb, print_level);
        cp_list_iterator_init(&iter, ancestors, COLLECTION_LOCK_NONE);
        while ((ancestor = cp_list_iterator_next(&iter)))
        {
            if (ancestor->level <= 1)
                continue;
            switch (ancestor->type)
            {
            case YMLDB_BRANCH:
                fprintf(stream, "%.*s%s:\n", (ancestor->level - 2) * 2, g_space, ancestor->key);
                break;
            case YMLDB_LEAFLIST:
                fprintf(stream, "%.*s- %s\n", (ancestor->level - 2) * 2, g_space, ancestor->key);
                break;
            case YMLDB_LEAF:
                fprintf(stream, "%.*s%s: %s\n", (ancestor->level - 2) * 2, g_space, ancestor->key, ancestor->value);
                break;
            default:
                _log_error("unknown type?!??? %d\n", ancestor->type);
                break;
            }
        }
        _ymldb_traverse_free(ancestors);
    }

    if (params->update_callback_en)
    {
        if (ydb->callback && ydb->callback->type == YMLDB_UPDATE_CALLBACK)
            _update_callback_run(ydb);
    }

    if (ydb->type == YMLDB_BRANCH)
    {
        if (ydb->level > 1) // 0 and 1 is major_key.
        {                   // not print out for top node
            fprintf(stream, "%.*s%s:\n", (ydb->level - 2) * 2, g_space, ydb->key);
        }
        if (no_print_children)
            goto end;
        cp_avltree_callback(ydb->children, _ymldb_param_streambuffer_each_of_node, params);
    }
    else if (ydb->type == YMLDB_LEAFLIST)
    {
        fprintf(stream, "%.*s- %s\n", (ydb->level - 2) * 2, g_space, _str_dump(ydb->key));
    }
    else
    {
        fprintf(stream, "%.*s%s: %s\n", (ydb->level - 2) * 2, g_space, ydb->key, _str_dump(ydb->value));
    }
end:
    params->last_ydb = ydb;
    return;
}

// Return 1 if reply.stream is flushed, otherwise 0.
static int _params_buf_flush(struct ymldb_params *params, int forced)
{
    struct ystream *streambuffer = params->streambuffer;
    if (forced)
        goto flushing;
    else if (ftell(streambuffer->stream) >= YMLDB_STREAM_THRESHOLD)
        goto flushing;
    return 0;

flushing:
    _ymldb_fprintf_tail(streambuffer->stream);
    // write the stream to streambuffer->buf.
    _ystream_close(streambuffer);

    if (!forced)
    {
        if (params->out.opcode & YMLDB_OP_SEQ)
        {
            char *seq_tag = strstr(streambuffer->buf, YMLDB_TAG_SEQ_BASE);
            if (seq_tag)
                strncpy(seq_tag, YMLDB_TAG_SEQ_CON, strlen(YMLDB_TAG_SEQ_CON));
        }
    }

    _log_debug("@@ %zd %s\n\n", streambuffer->len, streambuffer->buf);

    _log_debug("@@ inprogress_cnt %d\n", params->cb->inprogress_cnt);
    if (params->cb->inprogress_cnt > 1)
    {
        // sub operation output is disabled..
        _log_debug("@@ ignored output.\n");
        return 1;
    }

    if (params->out.stream)
    {
        fputs(streambuffer->buf, params->out.stream);
        fflush(params->out.stream);
    }

    if (params->cb->flags & YMLDB_FLAG_CONN)
        _distribution_send(params);
    return 1;
}

static int _params_buf_reset(struct ymldb_params *params)
{
    struct ystream *streambuffer = params->streambuffer;
    _ystream_close(streambuffer);
    _ystream_open(params->streambuffer, "w");
    _ymldb_fprintf_head(params->streambuffer->stream, params->out.opcode, params->out.sequence);
    return 0;
}

static int _ymldb_run(struct ymldb_cb *cb, FILE *instream, FILE *outstream)
{
    int res = 0;
    int done = 0;
    struct ymldb_params *params;
    if (!cb)
    {
        _log_error("invalid ymldb_cb\n");
        return -1;
    }
    if (!instream)
    {
        _log_error("invalid input stream\n");
        return -1;
    }
    params = _params_alloc(cb, instream, outstream);
    if (!params)
    {
        return -1;
    }
    cb->inprogress_cnt++;
    _log_debug("major_key %s\n", cb->key);
    _log_debug(">>>\n");
    while (!done)
    {
        yaml_node_t *yroot = NULL;
        /* Get the next ymldb document. */
        res = _params_document_load(params);
        if (res < 0)
        {
            params->res--;
            break;
        }
        _params_opcode_extract(params);
        yroot = yaml_document_get_root_node(&params->document);
        if (yroot)
        {
            enum internal_op iop;
            iop = _ymldb_sm(params);
            // sync wait - done
            if (cb->flags & YMLDB_FLAG_INSYNC)
                if (!(params->in.opcode & YMLDB_OP_SEQ_CON))
                    cb->flags = cb->flags & (~YMLDB_FLAG_INSYNC);

            if (iop == iop_ignore)
            {
                _log_debug("in %uth %s\n", params->in.sequence, "ignored ...");
                params->res--;
                continue;
            }

            _params_buf_init(params);
            if (iop == iop_merge)
                _ymldb_internal_merge(params, cb->ydb, 1, 1);
            else if (iop == iop_delete)
                _ymldb_internal_delete(params, cb->ydb, 1, 1);
            else if (iop == iop_get)
                _ymldb_internal_get(params, cb->ydb, 1, 1);
            else if (iop == iop_relay)
                _ymldb_internal_relay(params, 0, 1, 1);
            else if (iop == iop_relay_delete)
            {
                _ymldb_internal_delete(params, cb->ydb, 1, 1);
                params->res = 0;           // reset failures.
                _params_buf_reset(params); // reset output buffer
                _ymldb_internal_relay(params, 0, 1, 1);
            }
            _params_buf_flush(params, 1); // forced flush!
            _log_debug("result: %s\n", params->res < 0 ? "failed" : "ok");
        }
        else
        {
            done = 1;
        }
    }
    _log_debug("<<<\n");
    res = params->res;
    _params_free(params);
    _notify_callback_run_pending();
    cb->inprogress_cnt--;
    return res;
}

int ymldb_run(char *major_key, FILE *instream, FILE *outstream)
{
    int res;
    struct ymldb_cb *cb;
    _log_entrance();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb or key found.\n");
        return -1;
    }
    if (!instream)
    {
        _log_error("invalid instream.\n");
        return -1;
    }
    _log_debug("\n");
    res = _ymldb_run(cb, instream, outstream);
    return res;
}

int ymldb_run_with_fd(char *major_key, int infd, int outfd)
{
    int res;
    FILE *instream;
    FILE *outstream;
    struct ymldb_cb *cb;
    _log_entrance();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb or key found.\n");
        return -1;
    }
    if (infd < 0)
    {
        _log_error("invalid infd.\n");
        return -1;
    }
    _log_debug("\n");
    if (outfd <= 0)
        outstream = NULL;
    else
        outstream = _ymldb_fopen_from_fd(outfd, "w");

    instream = _ymldb_fopen_from_fd(infd, "r");
    res = _ymldb_run(cb, instream, outstream);
    fclose(instream);
    if (outstream)
        fclose(outstream);
    return res;
}

int ymldb_is_created(char *major_key)
{
    struct ymldb_cb *cb;
    if (!(cb = _ymldb_cb(major_key)))
    {
        return 0;
    }
    return 1;
}

int ymldb_create(char *major_key, unsigned int flags)
{
    struct ymldb_cb *cb;
    _log_entrance();
    if (!major_key)
    {
        _log_error("no major key\n");
        return -1;
    }
    _log_debug("\n");
    // init top
    if (!g_ydb)
    {
        g_ydb = _ymldb_node_merge(NULL, NULL, YMLDB_BRANCH, "top", NULL);
        if (!g_ydb)
        {
            _log_error("g_ydb failed.\n");
            return -1;
        }
    }

    if (!g_ycb)
    {
        g_ycb = cp_avltree_create((cp_compare_fn)strcmp);
        if (!g_ycb)
        {
            _log_error("g_ycb failed.\n");
            return -1;
        }
    }

    if (cp_avltree_get(g_ydb->children, major_key))
    {
        _log_error("key exsits.\n");
        return -1;
    }

    cb = malloc(sizeof(struct ymldb_cb));
    if (!cb)
    {
        _log_error("alloc failed.\n");
        return -1;
    }
    memset(cb, 0x0, sizeof(struct ymldb_cb));
    cb->key = strdup(major_key);
    if (!cb->key)
    {
        _log_error("key alloc failed.\n");
        free(cb);
        return -1;
    }

    cb->flags = 0;
    cb->fd_publisher = -1;
    int i;
    for (i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
    {
        cb->fd_subscriber[i] = -1;
        cb->fd_flags[i] = 0;
    }

    cb->ydb = _ymldb_node_merge(NULL, g_ydb, YMLDB_BRANCH, major_key, NULL);
    if (!cb->ydb)
    {
        _log_error("init failed.\n");

        free(cb->key);
        free(cb);
        return -1;
    }

    _log_debug("major_key %s flags %x\n", major_key, flags);

    if (flags & YMLDB_FLAG_ASYNC)
        cb->flags |= YMLDB_FLAG_ASYNC;
    if (flags & YMLDB_FLAG_NO_RECORD)
        cb->flags |= YMLDB_FLAG_NO_RECORD;
    if (flags & (YMLDB_FLAG_PUBLISHER | YMLDB_FLAG_SUBSCRIBER | YMLDB_FLAG_SUB_PUBLISHER))
        _distribution_init(cb, flags);

    cp_avltree_insert(g_ycb, cb->key, cb);
    return 0;
}

static void _ymldb_destroy(void *data)
{
    struct ymldb_cb *cb = data;
    if (!cb)
        return;
    _log_debug("major_key %s\n", cb->key);
    if (cb->ydb)
    {
        struct ynode *ydb = cb->ydb;
        if (ydb->parent)
            cp_avltree_delete(cb->ydb->parent->children, ydb->key);
        _ymldb_node_free(ydb);
    }
    _distribution_deinit(cb);

    _log_debug("\n");
    if (cb->key)
        free(cb->key);
    _log_debug("\n");
    free(cb);
}

void ymldb_destroy(char *major_key)
{
    struct ymldb_cb *cb;
    _log_entrance();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb or key found.\n");
        return;
    }
    _log_debug("major_key %s\n", major_key);

    cp_avltree_delete(g_ycb, cb->key);
    _ymldb_destroy(cb);
    if (cp_avltree_count(g_ycb) <= 0)
    {
        _ymldb_node_free(g_ydb);
        cp_avltree_destroy(g_ycb);
        _log_debug("all destroyed ...\n");
        g_ycb = NULL;
        g_ydb = NULL;
    }
    _notify_callback_run_pending();
	_log_debug("done...\n");
}

void ymldb_destroy_all()
{
    _log_entrance();
    _log_debug("\n");
    if (g_ycb)
    {
        cp_avltree_destroy_custom(g_ycb, NULL, _ymldb_destroy);
        _ymldb_node_free(g_ydb);
        _log_debug("all destroyed ...\n");
        g_ycb = NULL;
        g_ydb = NULL;
    }
    _notify_callback_run_pending();
	_log_debug("done...\n");
}

static int _distribution_deinit(struct ymldb_cb *cb)
{
    if (!cb)
    {
        _log_error("no cb\n");
        return -1;
    }

    _log_debug("deinit conn g_fds=%p\n", g_fds);

    if (cb->fd_publisher >= 0)
    {
		if(g_fds) {
			cp_avltree_delete(g_fds, &cb->fd_publisher);
		}
        close(cb->fd_publisher);
        cb->fd_publisher = -1;
    }
    if (cb->flags & YMLDB_FLAG_PUBLISHER && !(cb->flags & YMLDB_FLAG_SUB_PUBLISHER))
    {
        int i;
        for (i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
        {
            if (cb->fd_subscriber[i] >= 0)
            {
				if(g_fds) {
					cp_avltree_delete(g_fds, &cb->fd_subscriber[i]);
				}
                close(cb->fd_subscriber[i]);
                cb->fd_subscriber[i] = -1;
                cb->fd_flags[i] = 0;
            }
        }
    }
    cb->flags = cb->flags & (~YMLDB_FLAG_CONN);
    cb->flags = cb->flags & (~YMLDB_FLAG_PUBLISHER);
    cb->flags = cb->flags & (~YMLDB_FLAG_SUBSCRIBER);
    cb->flags = cb->flags & (~YMLDB_FLAG_RECONNECT);
    cb->flags = cb->flags & (~YMLDB_FLAG_SUB_PUBLISHER);
    if (g_fds)
    {
		_log_debug("g_fds count %d\n", cp_avltree_count(g_fds));
        if (cp_avltree_count(g_fds) <= 0)
        {
            cp_avltree_destroy(g_fds);
            g_fds = NULL;
        }
    }
    return 0;
}

static int _g_fds_cmp(void *v1, void *v2)
{
    // struct ymldb_cb *cb1;
    // struct ymldb_cb *cb2;
    int *i1 = v1;
    int *i2 = v2;
    return *i1 - *i2;
}

static int _distribution_init(struct ymldb_cb *cb, int flags)
{
    int fd;
    char socketpath[128];
    struct sockaddr_un addr;
    _log_debug("\n");
    if (cb->flags & YMLDB_FLAG_CONN)
    {
        _distribution_deinit(cb);
    }
    if (!g_fds)
    {
        g_fds = cp_avltree_create((cp_compare_fn)_g_fds_cmp);
        if (!g_fds)
        {
            _log_error("g_fds failed.\n");
            return -1;
        }
    }
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        _log_error("%s socket failed (%s)\n", cb->key, strerror(errno));
        cb->flags |= YMLDB_FLAG_RECONNECT;
        return -1;
    }
    snprintf(socketpath, sizeof(socketpath), YMLDB_UNIXSOCK_PATH, cb->key);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketpath, sizeof(addr.sun_path) - 1);
    addr.sun_path[0] = 0;
    cb->fd_publisher = fd;
    if (flags & YMLDB_FLAG_ASYNC)
        cb->flags |= YMLDB_FLAG_ASYNC;
    if (flags & YMLDB_FLAG_PUBLISHER)
    { // PUBLISHER
        cb->flags |= YMLDB_FLAG_PUBLISHER;
        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            // _log_error("bind failed (%s).\n", strerror(errno));
            // if there is already binding publisher.
            if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
            {
                _log_error("%s connect failed (%s).\n", cb->key, strerror(errno));
                cb->flags |= YMLDB_FLAG_RECONNECT;
                return -1;
            }
            cb->flags |= YMLDB_FLAG_SUB_PUBLISHER;
            _log_debug("configured as a sub publisher\n");
            goto _done;
            // cb->flags |= YMLDB_FLAG_RECONNECT;
            // return -1;
        }
        if (listen(fd, YMLDB_SUBSCRIBER_MAX) < 0)
        {
            _log_error("%s listen failed (%s).\n", cb->key, strerror(errno));
            cb->flags |= YMLDB_FLAG_RECONNECT;
            return -1;
        }
    }
    else if (flags & YMLDB_FLAG_SUB_PUBLISHER)
    { // sub-publisher
        cb->flags |= YMLDB_FLAG_PUBLISHER;
        cb->flags |= YMLDB_FLAG_SUB_PUBLISHER;
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        {
            _log_error("%s connect failed (%s).\n", cb->key, strerror(errno));
            cb->flags |= YMLDB_FLAG_RECONNECT;
            return -1;
        }
    }
    else if (flags & YMLDB_FLAG_SUBSCRIBER)
    { // SUBSCRIBER
        cb->flags |= YMLDB_FLAG_SUBSCRIBER;
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        {
            _log_error("%s connect failed (%s).\n", cb->key, strerror(errno));
            cb->flags |= YMLDB_FLAG_RECONNECT;
            return -1;
        }
    }
_done:
	if(g_fds)
		cp_avltree_insert(g_fds, &cb->fd_publisher, cb);
    _log_debug("%s distribution - done (sock %d)\n", cb->key, fd);
    return fd;
}

static int _distribution_set(void *n, void *dummy)
{
    cp_avlnode *node = n;
    struct ymldb_cb *cb = node->value;
    struct ymldb_distribution *yd = dummy;
    int max = yd->max;
    fd_set *set = yd->set;
    // _log_debug("\n");
    if (cb->flags & YMLDB_FLAG_CONN)
    {
        if (cb->flags & YMLDB_FLAG_RECONNECT)
        {
            _log_debug("RECONN for major_key %s\n", cb->key);
            int res = _distribution_init(cb, cb->flags);
            if (res < 0)
                return 0;
        }
        if (cb->fd_publisher >= 0)
        {
            //_log_debug("cb %s fd_publisher %d\n", cb->key, cb->fd_publisher);
            FD_SET(cb->fd_publisher, set);
            max = cb->fd_publisher > max ? cb->fd_publisher : max;
        }
        if (cb->flags & YMLDB_FLAG_PUBLISHER && !(cb->flags & YMLDB_FLAG_SUB_PUBLISHER))
        {
            int i;
            for (i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
            {
                if (cb->fd_subscriber[i] >= 0)
                {
                    // _log_debug("cb %s fd_subscriber[%d] %d\n", cb->key, i, cb->fd_subscriber[i]);
                    FD_SET(cb->fd_subscriber[i], set);
                    max = cb->fd_subscriber[i] > max ? cb->fd_subscriber[i] : max;
                }
            }
        }
    }
    //_log_debug("max fd %d\n", max);
    yd->max = max;
    return 0;
}

static int _strfind_backward(char *src, ssize_t slen, char *searchstr)
{
    int searchstrlen = strlen(searchstr);
    int i = slen;
    int j = searchstrlen - 1;
    if (searchstrlen <= 0 || slen <= 0)
        return -1;
    for (; i >= 0; i--)
    {
        if (src[i] == searchstr[j])
        {
            j--;
        }
        else
        {
            j = searchstrlen - 1;
        }
        if (j < 0)
            break;
    }
    return i;
}

static int _distribution_recv(struct ymldb_cb *cb, FILE *outstream, int fd)
{
    int len = 0;
    struct ystream *input;
    cb->fd_requester = fd;
    if (cb->flags & YMLDB_FLAG_PUBLISHER && !(cb->flags & YMLDB_FLAG_SUB_PUBLISHER))
    {
        if (cb->fd_publisher == fd)
        {
            int i;
            fd = accept(cb->fd_publisher, NULL, NULL);
            if (fd < 0)
            {
                _log_error("accept failed (%s)\n", strerror(errno));
                goto _failed;
            }
            for (i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
            {
                if (cb->fd_subscriber[i] < 0)
                {
                    cb->fd_subscriber[i] = fd;
                    cb->fd_flags[i] = 0;
					if(g_fds)
	                    cp_avltree_insert(g_fds, &cb->fd_subscriber[i], cb);
                    _log_debug("subscriber (fd %d) added..\n", fd);
                    if (!(cb->flags & YMLDB_FLAG_ASYNC))
                        ymldb_sync_ack(cb->key);
                    break;
                }
            }
            if (i >= YMLDB_SUBSCRIBER_MAX)
            {
                _log_error("subscription over..\n");
                close(fd);
                goto _failed;
            }
            goto _done;
        }
    }
    input = _ystream_alloc(YMLDB_STREAM_BUF_SIZE);
    if (!input)
    {
        _log_error("fail to open ymldb stream\n");
        goto _failed;
    }
read_message:
    len = read(fd, input->buf + input->len, input->buflen - input->len);
    input->len += len;
    if (len <= 0)
    {
        if (len < 0)
            _log_error("fd %d read failed (%s)\n", fd, strerror(errno));
        else
            _log_error("fd %d closed (EOF)\n", fd);

        if (fd == cb->fd_publisher)
            cb->flags |= YMLDB_FLAG_RECONNECT;
        else
        {
            int i;
            _log_debug("\n");
			if(g_fds)
	            cp_avltree_delete(g_fds, &fd);
            for (i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
            {
                if (cb->fd_subscriber[i] == fd)
                {
                    _log_debug("\n");
                    cb->fd_subscriber[i] = -1;
                    cb->fd_flags[i] = 0;
                    close(fd);
                }
            }
        }
        _log_debug("\n");
        _ystream_free(input);
        _log_debug("\n");
        goto _failed;
    }
    input->buf[input->len] = 0;
    if (input->len > YMLDB_STREAM_THRESHOLD)
    {
        len = input->len;
        int end_of_doc = _strfind_backward(input->buf, input->len, "...");
        if (end_of_doc >= 0)
        {
            input->len = end_of_doc + 3;
            input->buf[input->len] = 0;
            _log_debug("len=%zd buf=\n%s\n", input->len, input->buf);
            _ystream_open(input, "r");
            _ymldb_run(cb, input->stream, outstream);

            strcpy(input->buf, &input->buf[end_of_doc + 4]);
            input->len = len - (end_of_doc + 4);
            _log_debug("len=%zd strlen=%zd\n", input->len, strlen(input->buf));
            goto read_message;
        }
        else
        {
            _log_debug("oversize message");
            // oversize message will be dropped.
            _ystream_free(input);
            goto _done;
        }
    }
    else
    {
        _log_debug("len=%zd buf=\n%s\n", input->len, input->buf);
        _ystream_open(input, "r");
        _ymldb_run(cb, input->stream, outstream);
        _ystream_free(input);
    }
_done:
    cb->fd_requester = 0;
    return fd;
_failed:
    cb->fd_requester = 0;
    return -1;
}

static int _distribution_recv_internal(struct ymldb_cb *cb, FILE *outstream, fd_set *set)
{
    int res;
    _log_debug("\n");
    if (!(cb->flags & YMLDB_FLAG_CONN))
    {
        _log_error("not a subscriber or publisher\n");
        return -1;
    }
    if (cb->flags & YMLDB_FLAG_RECONNECT)
    {
		if(cb->fd_publisher > 0)
			FD_CLR(cb->fd_publisher, set);
        return _distribution_init(cb, cb->flags);
    }
    if (cb->fd_publisher >= 0)
    {
        if (FD_ISSET(cb->fd_publisher, set))
        {
            FD_CLR(cb->fd_publisher, set);
            res = _distribution_recv(cb, outstream, cb->fd_publisher);
            if (res < 0)
                return -1;
        }
    }
    if (cb->flags & YMLDB_FLAG_PUBLISHER)
    {
        int i;
        for (i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
        {
            if (cb->fd_subscriber[i] < 0)
                continue;
            if (FD_ISSET(cb->fd_subscriber[i], set))
            {
                FD_CLR(cb->fd_subscriber[i], set);
                _distribution_recv(cb, outstream, cb->fd_subscriber[i]);
            }
        }
    }
    return 0;
}

static int _distribution_recv_each_of_cb(void *n, void *dummy)
{
    cp_avlnode *node = n;
    struct ymldb_cb *cb = node->value;
    struct ymldb_distribution *yd = dummy;
    yd->res += _distribution_recv_internal(cb, yd->stream, yd->set);
	return 0;
}

// available for subscriber
static int _sync_wait(struct ymldb_cb *cb, FILE *outstream)
{
    int res = 0;
    if (!cb)
    {
        _log_error("no cb or set\n");
        return -1;
    }
    if (!(cb->flags & YMLDB_FLAG_SUBSCRIBER))
    {
        _log_error("not a subscriber\n");
        return -1;
    }
    _log_debug("\n");
    if (cb->flags & YMLDB_FLAG_RECONNECT)
    {
        res = _distribution_init(cb, cb->flags);
        if (res < 0)
            return res;
    }
    if (cb->fd_publisher >= 0)
    {
        int is_insync;
        int retry = 0;
        fd_set set;
        double diff = 0;
        struct timeval tv;
        struct timeval before, after;

        tv.tv_sec = 0;
        tv.tv_usec = 333333; // 0.333333 sec;
        gettimeofday(&before, NULL);
    _recv:
        FD_ZERO(&set);
        FD_SET(cb->fd_publisher, &set);
        res = select(cb->fd_publisher + 1, &set, NULL, NULL, &tv);
        if (res < 0)
        {
            cb->flags |= YMLDB_FLAG_RECONNECT;
            _log_error("fd %d select failed (%s)\n", cb->fd_publisher, strerror(errno));
            return res;
        }
        if (res == 0)
        {
            if (retry < 3)
            {
                retry++;
                goto _recv;
            }
            _log_error("fd %d timeout\n", cb->fd_publisher);
            return -1;
        }
        cb->flags |= YMLDB_FLAG_INSYNC;
        res = _distribution_recv_internal(cb, outstream, &set);
        is_insync = cb->flags & YMLDB_FLAG_INSYNC;
        cb->flags &= (~YMLDB_FLAG_INSYNC);
        if (res < 0)
            return res;
        gettimeofday(&after, NULL);
        diff = (after.tv_sec - before.tv_sec) * 1000000.0;
        diff = diff + after.tv_usec - before.tv_usec;
        _log_debug("sync-wait %0.3fus\n", diff);

        if (is_insync)
        {
            if (diff < 1.0)
            {
                _log_debug("sync-wait again\n");
                goto _recv;
            }
            _log_error("sync-wait failed.\n");
            return -1;
        }
    }
    _log_debug("sync-wait done\n");
    return res;
}

static int _distribution_send(struct ymldb_params *params)
{
    int res = 0;
    int sent = 0;
    int retry = 0;
    struct ymldb_cb *cb = params->cb;
    struct ystream *streambuffer = params->streambuffer;
    if ((cb->flags & YMLDB_FLAG_RECONNECT) || (cb->fd_publisher < 0))
    {
        cb->flags |= YMLDB_FLAG_RECONNECT;
        _log_error("reconn '%s'.\n", cb->key);
        return -1;
    }

    if (params->out.opcode & (YMLDB_OP_MERGE | YMLDB_OP_DELETE))
    {
        if (params->no_change)
        {
            _log_debug("discarded due to no_change in ymldb\n");
            return 0;
        }
    }

    int fd = 0;
    if (params->request_and_reply)
    {
        _log_debug("request/reply\n");
        if (cb->flags & YMLDB_FLAG_SUBSCRIBER || cb->flags & YMLDB_FLAG_SUB_PUBLISHER)
            fd = cb->fd_publisher;
        else if (cb->flags & YMLDB_FLAG_PUBLISHER && !(cb->flags & YMLDB_FLAG_SUB_PUBLISHER))
            fd = cb->fd_requester;
        else
        {
            _log_debug("discarded due to no peer\n");
            return 0;
        }
        if(fd <= 0)
        {
            _log_debug("discarded due to unknown request/reply target (fd %d)\n",
                cb->fd_requester);
            return 0;
        }
    _relay_rewrite:
        res = write(fd, streambuffer->buf + sent, streambuffer->len - sent);
        if (res < 0)
        {
            cb->flags |= YMLDB_FLAG_RECONNECT;
            _log_error("fd %d send failed (%s)\n", fd, strerror(errno));
            return -1;
        }
        sent = res + sent;
        if (sent < streambuffer->len && retry < 3)
        {
            retry++;
            _log_debug("retry++\n");
            goto _relay_rewrite;
        }
    }
    
    if (params->send_relay)
    {
        _log_debug("send_relay\n");
        if (cb->flags & YMLDB_FLAG_SUB_PUBLISHER)
        {
            if(cb->fd_requester != cb->fd_publisher)
            {
            subscriber_rewrite:
                res = write(cb->fd_publisher, streambuffer->buf + sent, streambuffer->len - sent);
                if (res < 0)
                {
                    cb->flags |= YMLDB_FLAG_RECONNECT;
                    _log_error("fd %d send failed (%s)\n",
                               cb->fd_publisher, strerror(errno));
                    return -1;
                }
                sent = res + sent;
                if (sent < streambuffer->len && retry < 3)
                {
                    retry++;
                    _log_debug("retry++\n");
                    goto subscriber_rewrite;
                }
            }
        }
        else if (cb->flags & YMLDB_FLAG_PUBLISHER && !(cb->flags & YMLDB_FLAG_SUB_PUBLISHER))
        {
            int i;
            for (i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
            {
                // doesn't relay message to cb->fd_requester
                if (cb->fd_subscriber[i] >= 0 && cb->fd_requester != cb->fd_subscriber[i])
                {
                    sent = 0;
                    retry = 0;
                publisher_rewrite:
                    res = write(cb->fd_subscriber[i], streambuffer->buf + sent, streambuffer->len - sent);
                    if (res < 0)
                    {
                        _log_error("fd %d send failed (%s)\n",
                                   cb->fd_subscriber[i], strerror(errno));
                        close(cb->fd_subscriber[i]);
                        cb->fd_subscriber[i] = -1;
                        cb->fd_flags[i] = 0;
                        continue;
                    }
                    sent = res + sent;
                    if (sent < streambuffer->len && retry < 3)
                    {
                        retry++;
                        _log_debug("retry++\n");
                        goto publisher_rewrite;
                    }
                }
            }
        }
    }
    return 0;
}

int ymldb_push(char *major_key, char *format, ...)
{
    int res;
    struct ystream *input;
    unsigned int opcode = YMLDB_OP_MERGE;
    struct ymldb_cb *cb;
    _log_entrance();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb or key found.\n");
        return -1;
    }
    _log_debug("\n");
    input = _ystream_open_dynamic();
    if (!input)
    {
        _log_error("fail to open ymldb stream\n");
        return -1;
    }
    // write ymldb data to the streambuf
    _ymldb_fprintf_head(input->stream, opcode, 0);
    va_list args;
    va_start(args, format);
    vfprintf(input->stream, format, args);
    va_end(args);
    _ymldb_fprintf_tail(input->stream);

    _ystream_open(input, "r");
    if (!input->stream)
    {
        _log_error("fail to open ymldb stream");
        _ystream_free(input);
        return -1;
    }
    _log_debug("input->len=%zd buf=\n%s\n", input->len, input->buf);
    res = _ymldb_run(cb, input->stream, NULL);
    _log_debug("result: %s\n", res < 0 ? "failed" : "ok");
    _ystream_free(input);
    return res;
}

static void _ymldb_remove_specifiers(FILE *dest, char *src)
{
    static char *specifiers_of_fscanf = "iudoxfegacsp";
    int specifier_pos = -1;
    int i;
    for (i = 0; src[i] != 0; i++)
    {
        if (specifier_pos > -1)
        {
            // _log_debug("i=%d, %c\n", i, src[i]);
            if (src[i] == ' ' || src[i] == '\n' || src[i] == '\t')
            {
                // _log_debug("i=%d, %c\n", i, src[i]);
                specifier_pos = -1;
                fputc(src[i], dest);
            }
            else if (strchr(specifiers_of_fscanf, src[i]))
            {
                // _log_debug("i=%d, %c\n", i, src[i]);
                specifier_pos = -1;
            }
            continue;
        }
        else if (src[i] == '%')
        {
            // _log_debug("i=%d, %c\n", i, src[i]);
            if (src[i + 1] == '%')
            {
                // _log_debug("i=%d, %c\n", i, src[i]);
                fputc(src[i], dest);
                fputc(src[i + 1], dest);
                i++;
                continue;
            }
            specifier_pos = i;
            continue;
        }
        else if (src[i] == '-')
        { // "- ""
            if (src[i + 1] == ' ')
            { // leaflist ymldb.
                fputc(' ', dest);
                continue;
            }
        }
        fputc(src[i], dest);
    }
}

// ymldb_pull supports to read ymldb as like scanf().
// not support - specifier option! (*, width, length...)
// %[*][width][length]specifier
// just %d, %c, %s, %x etc..
int ymldb_pull(char *major_key, char *format, ...)
{
    int res = -1;
    int opcode = YMLDB_OP_GET;
    struct ystream *input = NULL;
    struct ystream *output = NULL;
    struct ymldb_cb *cb;
    _log_entrance();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb or key found.\n");
        return -1;
    }
    _log_debug("\n");
    input = _ystream_open_dynamic();
    if (!input)
    {
        _log_error("fail to open ymldb stream\n");
        goto failed;
    }
    output = _ystream_open_dynamic();
    if (!output)
    {
        _log_error("fail to open ymldb stream\n");
        goto failed;
    }

    _ymldb_fprintf_head(input->stream, opcode, 0);
    _ymldb_remove_specifiers(input->stream, format);
    _ymldb_fprintf_tail(input->stream);

    _ystream_open(input, "r");
    if (!input->stream)
    {
        _log_error("fail to open ymldb stream");
        goto failed;
    }

    _log_debug("input->len=%zd buf=\n%s\n", input->len, input->buf);
    res = _ymldb_run(cb, input->stream, output->stream);
    _log_debug("result: %s\n", res < 0 ? "failed" : "ok");
    _log_debug("output->len=%zd buf=\n%s\n", output->len, output->buf);
    if (res >= 0)
    { // success
        char *doc_body = strstr(output->buf, "---");
        if (doc_body)
            doc_body = doc_body + 4;
        else
            doc_body = output->buf;
        va_list args;
        va_start(args, format);
        vsscanf(doc_body, format, args);
        va_end(args);
    }
failed:
    _ystream_free(input);
    _ystream_free(output);
    return res;
}

// write a key and value
int _ymldb_write(FILE *outstream, unsigned int opcode, char *major_key, ...)
{
    int res;
    int level = 0;
    struct ystream *input;
    struct ymldb_cb *cb;
    _log_entrance();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb or key found.\n");
        return -1;
    }
    if (opcode == 0)
    {
        _log_error("no opcode\n");
        return -1;
    }
    _log_debug("major_key %s\n", cb->key);
    _log_debug("opcode %s\n", _opcode_str(opcode));
    input = _ystream_open_dynamic();
    if (!input)
    {
        _log_error("fail to open ymldb stream\n");
        return -1;
    }

    _ymldb_fprintf_head(input->stream, opcode, 0);
    // fprintf(input->stream, "%.*s%s:\n", level * 2, g_space, major_key);
    // level++;

    va_list args;
    va_start(args, major_key);
    char *cur_token;
    char *next_token;
    cur_token = va_arg(args, char *);
    while (cur_token != NULL)
    {
        next_token = va_arg(args, char *);
        if (!next_token)
        {
            fprintf(input->stream, "%.*s%s\n", level * 2, g_space, cur_token);
            break;
        }
        fprintf(input->stream, "%.*s%s:\n", level * 2, g_space, cur_token);
        cur_token = next_token;
        level++;
    };
    va_end(args);
    _ymldb_fprintf_tail(input->stream);

    _ystream_open(input, "r");
    if (!input->stream)
    {
        _log_error("fail to open ymldb stream");
        _ystream_free(input);
        return -1;
    }
    _log_debug("len=%zd buf=\n%s\n", input->len, input->buf);
    res = _ymldb_run(cb, input->stream, outstream);
    _log_debug("result: %s\n", res < 0 ? "failed" : "ok");
    _ystream_free(input);
    if (res >= 0)
    {
        int sync = 0;
        if ((opcode & YMLDB_OP_SYNC) && cb->flags & YMLDB_FLAG_SUBSCRIBER)
            sync = 1;
        if ((opcode & YMLDB_OP_GET) && (cb->flags & YMLDB_FLAG_SUBSCRIBER) && (cb->flags & YMLDB_FLAG_ASYNC))
            sync = 1;

        if (sync)
        {
            res = _sync_wait(cb, outstream);
        }
    }
    return res;
}

// write a key and value
int _ymldb_write2(FILE *outstream, unsigned int opcode, int keys_num, char *keys[])
{
    int res;
    int level = 0;
    struct ystream *input;
    struct ymldb_cb *cb;
    _log_entrance();
    if (keys_num <= 0 || keys == NULL)
    {
        _log_error("no key\n");
        return -1;
    }
    if (!(cb = _ymldb_cb(keys[0])))
    {
        _log_error("no ymldb or key (%s) found.\n", keys[0]);
        return -1;
    }
    if (opcode == 0)
    {
        _log_error("no opcode\n");
        return -1;
    }

    _log_debug("major_key %s\n", keys[0]);
    _log_debug("opcode %s\n", _opcode_str(opcode));
    input = _ystream_open_dynamic();
    if (!input)
    {
        _log_error("fail to open ymldb stream\n");
        return -1;
    }

    _ymldb_fprintf_head(input->stream, opcode, 0);
    for (level = 1; level < keys_num; level++)
    {
        fprintf(input->stream, "%.*s%s%s\n", (level - 1) * 2, g_space,
                keys[level], (level + 1 == keys_num) ? "" : ":");
    }
    _ymldb_fprintf_tail(input->stream);

    _ystream_open(input, "r");
    if (!input->stream)
    {
        _log_error("fail to open ymldb stream");
        _ystream_free(input);
        return -1;
    }
    _log_debug("len=%zd buf=\n%s\n", input->len, input->buf);
    res = _ymldb_run(cb, input->stream, outstream);
    _log_debug("result: %s\n", res < 0 ? "failed" : "ok");
    _ystream_free(input);
    if (res >= 0)
    {
        int sync = 0;
        if ((opcode & YMLDB_OP_SYNC) && cb->flags & YMLDB_FLAG_SUBSCRIBER)
            sync = 1;
        if ((opcode & YMLDB_OP_GET) && (cb->flags & YMLDB_FLAG_SUBSCRIBER) && (cb->flags & YMLDB_FLAG_ASYNC))
            sync = 1;

        if (sync)
        {
            res = _sync_wait(cb, outstream);
        }
    }
    return res;
}

// read a value by a key.
char *_ymldb_read(char *major_key, ...)
{ // directly access to ymldb.
    struct ynode *ydb;
    struct ymldb_cb *cb;
    _log_entrance();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb or key found.\n");
        return NULL;
    }
    ydb = cb->ydb;
    _log_debug("key %s\n", ydb->key);
    char *cur_token;
    char *next_token;
    va_list args;
    va_start(args, major_key);
    cur_token = va_arg(args, char *);
    while (cur_token != NULL)
    {
        _log_debug("key %s\n", cur_token);
        next_token = va_arg(args, char *);
        if (next_token)
        {
            if (ydb)
            {
                if (ydb->type == YMLDB_BRANCH)
                {
                    ydb = cp_avltree_get(ydb->children, cur_token);
                }
                else
                { // not exist..
                    ydb = NULL;
                    break;
                }
            }
            else
            {
                // not exist..
                break;
            }
        }
        else
        {
            if (ydb)
            {
                if (ydb->type == YMLDB_BRANCH)
                {
                    ydb = cp_avltree_get(ydb->children, cur_token);
                }
            }
        }
        cur_token = next_token;
    };
    va_end(args);
    if (ydb)
    {
        if (ydb->type == YMLDB_BRANCH)
            return NULL;
        return ydb->value;
    }
    else
        return NULL;
}

// read a value by a key.
char *_ymldb_read2(int keys_num, char *keys[])
{ // directly access to ymldb.
    int i;
    struct ynode *ydb;
    struct ymldb_cb *cb;
    _log_entrance();
    if (keys_num <= 0 || keys == NULL)
    {
        _log_error("no key\n");
        return NULL;
    }
    if (!(cb = _ymldb_cb(keys[0])))
    {
        _log_error("no ymldb or key (%s) found.\n", keys[0]);
        return NULL;
    }
    ydb = cb->ydb;
    _log_debug("key %s\n", ydb->key);
    for (i = 1; i < keys_num; i++)
    {
        if (ydb)
        {
            if (ydb->type == YMLDB_BRANCH)
                ydb = cp_avltree_get(ydb->children, keys[i]);
            else
                return NULL;
        }
        else
            return NULL;
    }
    if (ydb)
    {
        if (ydb->type == YMLDB_BRANCH)
            return NULL;
        return ydb->value;
    }
    else
        return NULL;
}

void ymldb_dump_all(FILE *outstream, char *major_key)
{
    if (!outstream)
        return;
    if (major_key)
    {
        struct ymldb_cb *cb;
        if (!(cb = _ymldb_cb(major_key)))
        {
            return;
        }
        fprintf(outstream, "\n [Current ymldb tree]\n\n");
        _ymldb_fprintf_node(outstream, cb->ydb, 0, 0);
        fprintf(outstream, "\n  @@ g_alloc_count %d @@\n\n", g_alloc_count);
    }
    else
    { // print all..
        fprintf(outstream, "\n [Current ymldb tree]\n\n");
        _ymldb_fprintf_node(outstream, g_ydb, 0, 0);
        fprintf(outstream, "\n  @@ g_alloc_count %d @@\n\n", g_alloc_count);
    }
}

int ymldb_distribution_deinit(char *major_key)
{
    struct ymldb_cb *cb;
    _log_entrance();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb or key found.\n");
        return -1;
    }
    return _distribution_deinit(cb);
}

int ymldb_distribution_init(char *major_key, int flags)
{
    struct ymldb_cb *cb;
    _log_entrance();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb or key found.\n");
        return -1;
    }
    return _distribution_init(cb, flags);
}

int ymldb_distribution_set(fd_set *set)
{
    struct ymldb_distribution yd;
    yd.set = set;
    yd.max = 0;
	yd.res = 0;
    _log_entrance();
    if (!set)
    {
        _log_error("no fd_set\n");
        return -1;
    }
    if (g_ycb)
        cp_avltree_callback(g_ycb, _distribution_set, &yd);
    return yd.max;
}

int ymldb_distribution_recv_and_dump(FILE *outstream, fd_set *set)
{
    struct ymldb_distribution yd;
    yd.set = set;
    yd.max = 0;
    yd.stream = outstream;
	yd.res = 0;
    _log_entrance();
    if (!set)
    {
        _log_error("no fd_set\n");
        return -1;
    }
    if (g_ycb)
        cp_avltree_callback(g_ycb, _distribution_recv_each_of_cb, &yd);
	_log_debug("res=%d\n", yd.res);
    return yd.res;
}

int ymldb_distribution_recv(fd_set *set)
{
    return ymldb_distribution_recv_and_dump(NULL, set);
}

int ymldb_distribution_recv_fd_and_dump(FILE *outstream, int *cur_fd)
{
    struct ymldb_cb *cb;
    _log_entrance();
    if (!cur_fd)
    {
        _log_error("no cur_fd\n");
        return -1;
    }
	
	if(g_fds)
	{
		cb = cp_avltree_get(g_fds, cur_fd);
		if (cb)
		{
			int res = _distribution_recv(cb, outstream, *cur_fd);
			if (res < 0)
			{
				*cur_fd = -1;
			}
			_log_debug("\n");
			return res;
		}
	}

    _log_error("unknown fd (%d) \n", *cur_fd);
    *cur_fd = -1;
    return -1;
}

int ymldb_distribution_recv_fd(int *cur_fd)
{
    return ymldb_distribution_recv_fd_and_dump(NULL, cur_fd);
}

int ymldb_distribution_add(char *major_key, int subscriber_fd)
{
    struct ymldb_cb *cb;
    _log_entrance();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb or key found.\n");
        return -1;
    }
    if (!(cb->flags & YMLDB_FLAG_PUBLISHER))
    {
        _log_error("this is available for a publisher ymldb.\n");
        return -1;
    }
    if (cb->flags & YMLDB_FLAG_RECONNECT)
    {
        _log_error("unable to add a subscriber fd within RECONN.\n");
        return -1;
    }
    int i;
    for (i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
    {
        if (cb->fd_subscriber[i] < 0)
        {
            cb->fd_subscriber[i] = subscriber_fd;
            cb->fd_flags[i] = 0;
            _log_debug("subscriber (fd %d) added..\n", subscriber_fd);
            if (!(cb->flags & YMLDB_FLAG_ASYNC))
                ymldb_sync_ack(cb->key);
            break;
        }
        if (i >= YMLDB_SUBSCRIBER_MAX)
        {
            _log_error("subscription over..\n");
            return -1;
        }
    }
    return 0;
}

int ymldb_distribution_delete(char *major_key, int subscriber_fd)
{
    struct ymldb_cb *cb;
    _log_entrance();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb or key found.\n");
        return -1;
    }
    if (!(cb->flags & YMLDB_FLAG_PUBLISHER))
    {
        _log_error("this is available for a publisher ymldb.\n");
        return -1;
    }
    if (cb->flags & YMLDB_FLAG_RECONNECT)
    {
        _log_error("unable to add a subscriber fd within RECONN.\n");
        return -1;
    }
    if (subscriber_fd < 0)
    {
        _log_error("invalid fd.\n");
        return 0;
    }
    int i;
    for (i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
    {
        if (cb->fd_subscriber[i] == subscriber_fd)
        {
            cb->fd_subscriber[i] = -1;
            cb->fd_flags[i] = 0;
            _log_debug("subscriber (fd %d) deleted..\n", subscriber_fd);
            break;
        }
    }
    return 0;
}

static struct ycallback *_callback_alloc(
    int type, ymldb_callback_fn usr_func, void *usr_data, struct ynode *ydb)
{
    struct ycallback *callback;
    callback = malloc(sizeof(struct ycallback));
    if (!callback)
        return NULL;
    // memset(callback, 0, sizeof(struct ycallback));
    callback->ydb = ydb;
    callback->usr_data = usr_data;
    callback->usr_func = usr_func;
    callback->meta_data = NULL;
    callback->deleted = 0;
    callback->type = type;
    _log_debug("callback %p alloc\n", callback);
    return callback;
}

static void _callback_data_free(struct ymldb_callback_data *cd)
{
    if (cd)
    {
        int i;
        for (i = 0; i < YMLDB_CALLBACK_MAX; i++)
        {
            if (cd->keys[i])
                free(cd->keys[i]);
        }
        if (cd->value)
            free(cd->value);
        free(cd);
    }
}

static void _callback_free(struct ycallback *callback)
{
    _log_debug("callback %p free\n", callback);
    if (callback)
    {
        _callback_data_free(callback->meta_data);
        free(callback);
    }
}

int _ymldb_callback_register(int type, ymldb_callback_fn usr_func, void *usr_data, char *major_key, ...)
{
    int i = 0;
    int max = 0;
    char *key[YMLDB_CALLBACK_MAX + 1];
    struct ynode *ydb;
    struct ymldb_cb *cb;
    _log_entrance();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb or key found.\n");
        return -1;
    }
    if (!usr_func)
    {
        _log_error("no usr_func\n");
        return -1;
    }

    char *cur_token;
    va_list args;
    va_start(args, major_key);
    key[i++] = major_key;
    cur_token = va_arg(args, char *);
    while (cur_token != NULL)
    {
        _log_debug("key %s\n", cur_token);
        if (i < YMLDB_CALLBACK_MAX)
            key[i++] = cur_token;
        else
        {
            _log_error("The callback key depth is over. (support key depth up to %d.)\n", YMLDB_CALLBACK_MAX);
            return -1;
        }
        cur_token = va_arg(args, char *);
    }
    key[i] = NULL;
    va_end(args);

    max = i;
    ydb = cb->ydb;
    // _log_debug("key %s\n", ydb->key);
    for (i = 1; i < max; i++)
    {
        struct ynode *p_ydb = ydb;
        if (p_ydb->type != YMLDB_BRANCH)
        {
            _log_error("usr_func is unable to be registered to ymldb leaf.!\n");
            return -1;
        }
        ydb = cp_avltree_get(p_ydb->children, key[i]);
        if (!ydb)
        {
            ydb = _ymldb_node_merge(NULL, p_ydb, YMLDB_BRANCH, key[i], NULL);
            if (!ydb)
            {
                _log_error("fail to register usr_func!\n");
                return -1;
            }
        }
    }

    if (ydb)
    {
        // if (ydb->type != YMLDB_BRANCH)
        // {
        //     _log_error("usr_func can be registered to ymldb branch.!\n");
        //     return -1;
        // }
        if (ydb->callback)
        {
            _callback_free(ydb->callback);
        }
        ydb->callback = _callback_alloc(type, usr_func, usr_data, ydb);
        if (ydb->callback)
        {
            _log_debug("callback %p registered...\n", ydb->callback);
            return 0;
        }
    }
    _log_error("fail to register usr_func..\n");
    return -1;
}

int _ymldb_callback_unregister(char *major_key, ...)
{
    struct ynode *ydb;
    struct ymldb_cb *cb;
    _log_entrance();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb or key found.\n");
        return -1;
    }
    ydb = cb->ydb;
    _log_debug("key %s\n", ydb->key);
    char *cur_token;
    va_list args;
    va_start(args, major_key);
    cur_token = va_arg(args, char *);
    while (cur_token)
    {
        _log_debug("key %s\n", cur_token);
        if (ydb->type != YMLDB_BRANCH)
        {
            _log_error("invalid ydb '%s'.\n", ydb->key);
            return -1;
        }
        ydb = cp_avltree_get(ydb->children, cur_token);
        if (!ydb)
        {
            _log_error("invalid key '%s'.\n", cur_token);
            return -1;
        }
        cur_token = va_arg(args, char *);
    }
    va_end(args);
    if (ydb)
    {
        if (ydb->callback)
        {
            _callback_free(ydb->callback);
        }
        ydb->callback = NULL;
        return 0;
    }
    _log_error("unreachable here.\n");
    return -1;
}

int _ymldb_callback_register2(
    int type, ymldb_callback_fn usr_func, void *usr_data, int keys_num, char *keys[])
{
    int i;
    struct ynode *ydb;
    struct ymldb_cb *cb;
    _log_entrance();
    if (keys_num <= 0 || keys == NULL)
    {
        _log_error("no key\n");
        return -1;
    }
    if (!(cb = _ymldb_cb(keys[0])))
    {
        _log_error("no ymldb or key (%s) found.\n", keys[0]);
        return -1;
    }
    ydb = cb->ydb;
    _log_debug("key %s\n", ydb->key);

    for (i = 1; i < keys_num; i++)
    {
        struct ynode *p_ydb = ydb;
        if (p_ydb->type != YMLDB_BRANCH)
        {
            _log_error("usr_func is unable to be registered to ymldb leaf.!\n");
            return -1;
        }
        ydb = cp_avltree_get(p_ydb->children, keys[i]);
        if (!ydb)
        {
            ydb = _ymldb_node_merge(NULL, p_ydb, YMLDB_BRANCH, keys[i], NULL);
            if (!ydb)
            {
                _log_error("fail to register usr_func!\n");
                return -1;
            }
        }
        _log_debug("key %s\n", ydb->key);
    }

    if (ydb)
    {
        // if (ydb->type != YMLDB_BRANCH)
        // {
        //     _log_error("usr_func can be registered to ymldb branch.!\n");
        //     return -1;
        // }
        if (ydb->callback)
        {
            _callback_free(ydb->callback);
        }
        ydb->callback = _callback_alloc(type, usr_func, usr_data, ydb);
        if (ydb->callback)
        {
            _log_debug("callback %p registered...\n", ydb->callback);
            return 0;
        }
    }
    _log_error("fail to register usr_func..\n");
    return -1;
}

int _ymldb_callback_unregister2(int keys_num, char *keys[])
{
    int i;
    struct ynode *ydb;
    struct ymldb_cb *cb;
    _log_entrance();
    if (keys_num <= 0 || keys == NULL)
    {
        _log_error("no key\n");
        return -1;
    }
    if (!(cb = _ymldb_cb(keys[0])))
    {
        _log_error("no ymldb or key (%s) found.\n", keys[0]);
        return -1;
    }
    ydb = cb->ydb;
    _log_debug("key %s\n", ydb->key);

    for (i = 1; i < keys_num; i++)
    {
        if (ydb->type != YMLDB_BRANCH)
        {
            _log_error("invalid ydb '%s'.\n", ydb->key);
            return -1;
        }
        ydb = cp_avltree_get(ydb->children, keys[i]);
        if (!ydb)
        {
            _log_error("invalid key '%s'.\n", keys[i]);
            return -1;
        }
        _log_debug("key %s\n", ydb->key);
    }

    if (ydb)
    {
        if (ydb->callback)
            _callback_free(ydb->callback);
        ydb->callback = NULL;
        return 0;
    }
    _log_error("unreachable here.\n");
    return -1;
}

static void _notify_callback_pending(struct ynode *ydb, int deleted, int unregistered)
{
    int i;
    struct ycallback *callback = NULL;
    struct ymldb_callback_data *cd = NULL;
    if (ydb->level >= YMLDB_CALLBACK_MAX)
    {
        _log_error("Supported key level of a callback is up to %d.\n", YMLDB_CALLBACK_MAX);
        return;
    }
    cd = malloc(sizeof(struct ymldb_callback_data));
    if (!cd)
    {
        _log_error("alloc ymldb_callback_data failed.\n");
        return;
    }
    cd->keys_num = ydb->level;
    for (i = 0; i < YMLDB_CALLBACK_MAX; i++)
        cd->keys[i] = NULL;
    cd->deleted = deleted;
    cd->unregistered = unregistered;
    cd->value = (ydb->type != YMLDB_BRANCH) ? (strdup(ydb->value)) : NULL; //copy!
    cd->type = YMLDB_NOTIFY_CALLBACK;
    _log_debug("cd->value = %s\n", cd->value);
    do
    {
        // ignore unexpected level.
        if (ydb->level <= 0 || ydb->level >= YMLDB_CALLBACK_MAX)
            break;
        cd->keys[ydb->level - 1] = strdup(ydb->key); //copy!
        if (!callback && ydb->callback && ydb->callback->type == YMLDB_NOTIFY_CALLBACK)
        {
            callback = ydb->callback;
            cd->keys_level = ydb->level;
        }
        ydb = ydb->parent;
    } while (ydb);

    if (callback)
    {
        _log_debug("\n");
        struct ycallback *new_callback = NULL;
        if (!g_callbacks)
            g_callbacks = cp_list_create_nosync();
        new_callback = _callback_alloc(callback->type, callback->usr_func, callback->usr_data, callback->ydb);
        if (new_callback && g_callbacks)
        {
            new_callback->meta_data = cd;
            cp_list_append(g_callbacks, new_callback);
            return;
        }
        _callback_free(new_callback);
    }
    // if failed...
    _callback_data_free(cd);
    return;
}

static void _notify_callback_run(struct ynode *ydb, int deleted)
{
    int i;
    struct ynode *origin = ydb;
    struct ymldb_callback_data cdata;
    struct ycallback *callback = NULL;
    if (ydb->level >= YMLDB_CALLBACK_MAX)
    {
        _log_error("Supported key level of a callback is up to %d.\n", YMLDB_CALLBACK_MAX);
        return;
    }
    cdata.keys_num = ydb->level;
    for (i = 0; i < YMLDB_CALLBACK_MAX; i++)
        cdata.keys[i] = NULL;
    cdata.deleted = deleted;
    cdata.unregistered = 0;
    cdata.value = (ydb->type != YMLDB_BRANCH) ? ydb->value : NULL;
    cdata.type = YMLDB_NOTIFY_CALLBACK;
    do
    {
        if (ydb->level <= 0 || ydb->level >= YMLDB_CALLBACK_MAX)
            break;

        cdata.keys[ydb->level - 1] = ydb->key;
        if (!callback && ydb->callback && ydb->callback->type == YMLDB_NOTIFY_CALLBACK)
        {
            callback = ydb->callback;
            cdata.keys_level = ydb->level;
        }
        ydb = ydb->parent;
    } while (ydb);
    if (callback)
    {
        callback->usr_func(callback->usr_data, &cdata);
        // callback for deleting itself.
        if (deleted)
        {
            if (origin == callback->ydb)
                callback->deleted = 1;
        }
    }
}

static void _notify_callback_run_pending()
{
    struct ycallback *callback = NULL;
    if (!g_callbacks)
        return;
    _log_debug("callback (cnt=%d)\n", (int)cp_list_item_count(g_callbacks));
    while ((callback = cp_list_remove_head(g_callbacks)) != NULL)
    {
        callback->usr_func(callback->usr_data, callback->meta_data);
        _callback_free(callback);
    }
    cp_list_destroy(g_callbacks);
    g_callbacks = NULL;
}

static void _callback_unreg(struct ynode *ydb, int del)
{
    _log_debug("%s\n", ydb->key);
    // append the unregistering callback to execute the end of processing.
    _notify_callback_pending(ydb, del, 1);
    // free current callback.
    _callback_free(ydb->callback);
}

static void _update_callback_run(struct ynode *ydb)
{
    int i;
    struct ymldb_callback_data cdata;
    struct ycallback *callback = ydb->callback;
    if (ydb->level >= YMLDB_CALLBACK_MAX)
    {
        _log_error("Supported key level of a callback is up to %d.\n", YMLDB_CALLBACK_MAX);
        return;
    }
    if (!callback || callback->type != YMLDB_UPDATE_CALLBACK)
    {
        _log_error("Not update callback\n");
        return;
    }
    cdata.keys_num = ydb->level;
    cdata.keys_level = ydb->level;
    for (i = 0; i < YMLDB_CALLBACK_MAX; i++)
        cdata.keys[i] = NULL;
    cdata.deleted = 0;
    cdata.unregistered = 0;
    cdata.value = (ydb->type != YMLDB_BRANCH) ? ydb->value : NULL;
    cdata.type = YMLDB_UPDATE_CALLBACK;
    do
    {
        if (ydb->level <= 0 || ydb->level >= YMLDB_CALLBACK_MAX)
            break;
        cdata.keys[ydb->level - 1] = ydb->key;
        ydb = ydb->parent;
    } while (ydb);
    if (callback)
        callback->usr_func(callback->usr_data, &cdata);
}

struct ymldb_iterator *_ymldb_iterator_init(struct ymldb_iterator *iter, char *major_key, ...)
{
    struct ynode *ydb;
    struct ymldb_cb *cb;
    int iter_allocated = 0;
    _log_entrance();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb or key found.\n");
        return NULL;
    }
    ydb = cb->ydb;
    if (!iter)
    {
        iter = malloc(sizeof(struct ymldb_iterator));
        iter_allocated = 1;
    }
    if (!iter)
    {
        _log_error("ymldb_iterator alloc failed.\n");
        return NULL;
    }
    _log_debug("key %s\n", ydb->key);
    char *cur_token;
    va_list args;
    va_start(args, major_key);
    cur_token = va_arg(args, char *);
    while (cur_token != NULL)
    {
        _log_debug("key %s\n", cur_token);
        if (ydb)
        {
            if (ydb->type == YMLDB_BRANCH && ydb->children)
                ydb = cp_avltree_get(ydb->children, cur_token);
            else
                ydb = NULL;
        }
        else
        {
            break;
        }
        cur_token = va_arg(args, char *);
    };
    va_end(args);

    if (!ydb)
    {
        _log_error("no entry found\n");
        if(iter_allocated)
            free(iter);
        return NULL;
    }

    iter->ydb = (void *)ydb;
    iter->cur = (void *)ydb;
    return iter;
}

struct ymldb_iterator *_ymldb_iterator_init2(struct ymldb_iterator *iter, int keys_num, char *keys[])
{
    int level;
    struct ynode *ydb;
    struct ymldb_cb *cb;
    int iter_allocated = 0;
    _log_entrance();
    if (keys_num <= 0 || keys == NULL)
    {
        _log_error("no key\n");
        return NULL;
    }
    if (!(cb = _ymldb_cb(keys[0])))
    {
        _log_error("no ymldb or key (%s) found.\n", keys[0]);
        return NULL;
    }
    ydb = cb->ydb;
    if (!iter)
    {
        iter = malloc(sizeof(struct ymldb_iterator));
        iter_allocated = 1;
    }
    if (!iter)
    {
        _log_error("ymldb_iterator alloc failed.\n");
        return NULL;
    }
    _log_debug("key %s, keys_num %d\n", ydb->key, keys_num);
    for(level = 1; level < keys_num; level++)
    {
        if (ydb)
        {
            if (ydb->type == YMLDB_BRANCH && ydb->children)
            {
                ydb = cp_avltree_get(ydb->children, keys[level]);
            }
            else {
                ydb = NULL;
            }
        }
        else
        {
            break;
        }
    }

    if (!ydb)
    {
        _log_error("no entry found\n");
        if(iter_allocated)
            free(iter);
        return NULL;
    }

    iter->ydb = (void *)ydb;
    iter->cur = (void *)ydb;
    return iter;
}


void ymldb_iterator_deinit(struct ymldb_iterator *iter)
{
    if (!iter)
        return;
    memset(iter, 0, sizeof(struct ymldb_iterator));
}

void ymldb_iterator_free(struct ymldb_iterator *iter)
{
    _log_entrance();
    if (iter)
        free(iter);
}

int ymldb_iterator_reset(struct ymldb_iterator *iter)
{
    if (!iter)
        return -1;
    iter->cur = iter->ydb;
    return 0;
}

int ymldb_iterator_rebase(struct ymldb_iterator *iter)
{
    if (!iter)
        return -1;
    iter->ydb = iter->cur;
    return 0;
}

struct ymldb_iterator *ymldb_iterator_copy(struct ymldb_iterator *src)
{
    struct ymldb_iterator *dest = NULL;
    _log_entrance();
    if (!src)
    {
        _log_error("no src iterator\n");
        return NULL;
    }
    dest = malloc(sizeof(struct ymldb_iterator));
    if (dest)
    {
        memset(dest, 0, sizeof(struct ymldb_iterator));
        dest->ydb = src->ydb;
        dest->cur = src->cur;
    }
    return dest;
}

const char *ymldb_iterator_lookup_down(struct ymldb_iterator *iter, char *key)
{
    struct ynode *cur = NULL;
    struct ynode *child = NULL;
    if (!iter)
        return NULL;
    if (!key)
        key = "";
    cur = (struct ynode *)iter->cur;
    if (!cur)
        return NULL;
    if (cur->type == YMLDB_BRANCH)
    {
        child = cp_avltree_get(cur->children, (void *)key);
        if (child)
        {
            cur = child;
            iter->cur = (void *)cur;
            return cur->key;
        }
        return NULL;
    }
    return NULL;
}

const char *ymldb_iterator_lookup(struct ymldb_iterator *iter, char *key)
{
    struct ynode *next = NULL;
    struct ynode *cur = NULL;
    if (!iter)
        return NULL;
    cur = (struct ynode *)iter->cur;
    if (!cur)
        return NULL;
    if (cur->parent)
    {
        next = cp_avltree_get(cur->parent->children, (void *)key);
        if (next)
        {
            cur = next;
            iter->cur = (void *)cur;
            return cur->key;
        }
        return NULL;
    }
    return NULL;
}

const char *ymldb_iterator_down(struct ymldb_iterator *iter)
{
    struct ynode *cur = NULL;
    struct ynode *child = NULL;
    if (!iter)
        return NULL;
    cur = (struct ynode *)iter->cur;
    if (!cur)
        return NULL;
    if (cur->type == YMLDB_BRANCH)
    {
        child = cp_avltree_find(cur->children, "", CP_OP_GE);
        if (child)
        {
            cur = child;
            iter->cur = (void *)cur;
            return cur->key;
        }
        return NULL;
    }
    return NULL;
}

const char *ymldb_iterator_up(struct ymldb_iterator *iter)
{
    struct ynode *cur = NULL;
    if (!iter)
        return NULL;
    cur = (struct ynode *)iter->cur;
    if (!cur)
        return NULL;
    if (cur->parent)
    {
        cur = cur->parent;
        iter->cur = (void *)cur;
        return cur->key;
    }
    return NULL;
}

const char *ymldb_iterator_next(struct ymldb_iterator *iter)
{
    struct ynode *next = NULL;
    struct ynode *cur = NULL;
    if (!iter)
        return NULL;
    cur = (struct ynode *)iter->cur;
    if (!cur)
        return NULL;
    if (cur->parent)
    {
        next = cp_avltree_find(cur->parent->children, cur->key, CP_OP_GT);
        if (next)
        {
            cur = next;
            iter->cur = (void *)cur;
            return cur->key;
        }
        return NULL;
    }
    return NULL;
}

const char *ymldb_iterator_prev(struct ymldb_iterator *iter)
{
    struct ynode *prev = NULL;
    struct ynode *cur = NULL;
    if (!iter)
        return NULL;
    cur = (struct ynode *)iter->cur;
    if (!cur)
        return NULL;
    if (cur->parent)
    {
        prev = cp_avltree_find(cur->parent->children, cur->key, CP_OP_LT);
        if (prev)
        {
            cur = prev;
            iter->cur = (void *)cur;
            return cur->key;
        }
        return NULL;
    }
    return NULL;
}

const char *ymldb_iterator_get_value(struct ymldb_iterator *iter)
{
    struct ynode *cur = NULL;
    if (!iter)
        return NULL;
    cur = (struct ynode *)iter->cur;
    if (!cur)
        return NULL;
    if (cur->type == YMLDB_LEAF)
        return cur->value;
    else if (cur->type == YMLDB_LEAFLIST)
        return cur->key;
    return NULL;
}

const char *ymldb_iterator_get_key(struct ymldb_iterator *iter)
{
    struct ynode *cur = NULL;
    if (!iter)
        return NULL;
    cur = (struct ynode *)iter->cur;
    if (!cur)
        return NULL;
    return cur->key;
}


int ymldb_file_push(char *filename, char *format, ...)
{
    int res = 0;
    FILE *outstream;
    struct ystream *input = NULL;

    _log_entrance();
    if(!filename)
        return -1;
    if(ymldb_is_created(filename))
        return -2;
    outstream = fopen(filename, "w+");
    if(!outstream)
        return -3;
    res = ymldb_create(filename, YMLDB_FLAG_NONE);
    if(res < 0) {
        res = -4;
        goto _done;
    }

    _log_debug("\n");
    input = _ystream_open_dynamic();
    if (!input)
    {
        res = -5;
        goto _done;
    }
    // write ymldb data to the streambuf
    _ymldb_fprintf_head(input->stream, YMLDB_OP_MERGE, 0);
    va_list args;
    va_start(args, format);
    vfprintf(input->stream, format, args);
    va_end(args);
    _ymldb_fprintf_tail(input->stream);

    _ystream_open(input, "r");
    if (!input->stream)
    {
        res = -5;
        goto _done;
    }
    _log_debug("input->len=%zd buf=\n%s\n", input->len, input->buf);
    res = ymldb_run(filename, input->stream, outstream);
_done:
    _log_debug("result: %s\n", res < 0 ? "failed" : "ok");
    _ystream_free(input);
    ymldb_destroy(filename);
    // fflush(outstream);
    fclose(outstream);
    return res;
}

int ymldb_file_pull(char *filename, char *format, ...)
{
    int res;
    FILE *instream;
    struct ystream *input = NULL;
    struct ystream *output = NULL;
    
    _log_entrance();
    if(!filename)
        return -1;
    if(ymldb_is_created(filename))
        return -2;
    instream = fopen(filename, "r");
    if(!instream)
        return -3;
    res = ymldb_create(filename, YMLDB_FLAG_NONE);
    if(res < 0) {
        fclose(instream);
        return -4;
    }

    ymldb_run(filename, instream, NULL);
    fclose(instream);
    
    _log_debug("\n");
    input = _ystream_open_dynamic();
    if (!input)
    {
        res = -5;
        goto _done;
    }
    output = _ystream_open_dynamic();
    if (!output)
    {
        res = -6;
        goto _done;
    }

    _ymldb_fprintf_head(input->stream, YMLDB_OP_GET, 0);
    _ymldb_remove_specifiers(input->stream, format);
    _ymldb_fprintf_tail(input->stream);

    _ystream_open(input, "r");
    if (!input->stream)
    {
        _log_error("fail to open ymldb stream");
        goto _done;
    }

    _log_debug("input->len=%zd buf=\n%s\n", input->len, input->buf);
    res = ymldb_run(filename, input->stream, output->stream);
    if (res >= 0)
    { // success
        char *doc_body = strstr(output->buf, "---");
        if (doc_body)
            doc_body = doc_body + 4;
        else
            doc_body = output->buf;
        va_list args;
        va_start(args, format);
        vsscanf(doc_body, format, args);
        va_end(args);
    }
_done:
    _log_debug("output->len=%zd buf=\n%s\n", output->len, output->buf);
    _log_debug("result: %s\n", res < 0 ? "failed" : "ok");
    _ystream_free(input);
    _ystream_free(output);
    ymldb_destroy(filename);
    return res;
}
