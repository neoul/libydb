#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>

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

struct ymldb_callback
{
    ymldb_callback_fn usr_func;
    void *usr_data;
    int deleted;
};

struct ymldb
{
    char *key;
    int level;
    ymldb_type_t type;
    union {
        cp_avltree *children;
        char *value;
    };
    struct ymldb *parent;
    struct ymldb_callback *callback;
};

// #define OPEN_MEMSTREAM_ENABED
struct ymldb_stream
{
    FILE *stream;
    ssize_t buflen;
    ssize_t len;
    int is_write;
#ifdef OPEN_MEMSTREAM_ENABED
    char *buf;
#else
    char buf[];
#endif
};

// ymldb control block
#define YMLDB_SUBSCRIBER_MAX 8
struct ymldb_cb
{
    char *key;
    struct ymldb *ydb;
    unsigned int flags;
    // fd for YMLDB_FLAG_PUBLISHER and YMLDB_FLAG_SUBSCRIBER
    int fd_publisher;
    // fd for YMLDB_FLAG_PUBLISHER
    int fd_subscriber[YMLDB_SUBSCRIBER_MAX];
};

struct ymldb_distribution
{
    fd_set *set;
    int max;
    FILE *stream;
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
    int no_reply;
    int no_change;
    struct ymldb *last_ydb; // last updated ydb
    cp_avltree *callbacks;
    struct ymldb_stream *streambuffer;
};

#define _ENHANCED_

#define YMLDB_LOG_LOG 3
#define YMLDB_LOG_INFO 2
#define YMLDB_LOG_ERR 1
#define YMLDB_LOG_NONE 0
static int g_ymldb_log = YMLDB_LOG_LOG; // YMLDB_LOG_ERR;

#define _out(FP, ...) fprintf(FP, __VA_ARGS__)

#define _log_empty()                     \
    do                                   \
    {                                    \
        if (g_ymldb_log < YMLDB_LOG_LOG) \
            break;                       \
        fprintf(stdout, "\n\n>>>\n");    \
    } while (0)

#define _log_debug(...)                                                   \
    do                                                                    \
    {                                                                     \
        if (g_ymldb_log < YMLDB_LOG_LOG)                                  \
            break;                                                        \
        fprintf(stdout, "[ymldb:debug] %s:%d: ", __FUNCTION__, __LINE__); \
        fprintf(stdout, __VA_ARGS__);                                     \
    } while (0)

#define _log_info(...)                                                   \
    do                                                                   \
    {                                                                    \
        if (g_ymldb_log < YMLDB_LOG_INFO)                                \
            break;                                                       \
        fprintf(stdout, "[ymldb:info] %s:%d: ", __FUNCTION__, __LINE__); \
        fprintf(stdout, __VA_ARGS__);                                    \
    } while (0)

#define _log_error(...)                                         \
    do                                                          \
    {                                                           \
        if (g_ymldb_log < YMLDB_LOG_ERR)                      \
            break;                                              \
        fprintf(stderr, "\n  [ymldb:error]\n\n");               \
        fprintf(stderr, "\t%s:%d\n\t", __FUNCTION__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                           \
        fprintf(stderr, "\n");                                  \
    } while (0)

#define _log_error_head()                                     \
    do                                                        \
    {                                                         \
        if (g_ymldb_log < YMLDB_LOG_ERR)                    \
            break;                                            \
        fprintf(stderr, "\n  [ymldb:error]\n\n");             \
        fprintf(stderr, "\t%s:%d\n", __FUNCTION__, __LINE__); \
    } while (0)

#define _log_error_body(...)               \
    do                                     \
    {                                      \
        if (g_ymldb_log < YMLDB_LOG_ERR) \
            break;                         \
        fprintf(stderr, "\t");             \
        fprintf(stderr, __VA_ARGS__);      \
    } while (0)

#define _log_error_tail()                  \
    do                                     \
    {                                      \
        if (g_ymldb_log < YMLDB_LOG_ERR) \
            break;                         \
        fprintf(stderr, "\n");             \
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

struct ymldb_stream *ymldb_stream_alloc(size_t len);
void ymldb_stream_close(struct ymldb_stream *buf);
FILE *ymldb_stream_open(struct ymldb_stream *buf, char *rw);
void ymldb_stream_free(struct ymldb_stream *buf);
struct ymldb_stream *ymldb_stream_alloc_and_open(size_t len, char *rw);

static void _ymldb_params_free(struct ymldb_params *params);
static struct ymldb_params *_ymldb_params_alloc(struct ymldb_cb *cb, FILE *instream, FILE *outstream);
static int _ymldb_params_document_load(struct ymldb_params *params);
static int _ymldb_params_streambuffer_init(struct ymldb_params *params);
static int _ymldb_params_streambuffer_flush(struct ymldb_params *params, int forced);

static int _ymldb_distribution_deinit(struct ymldb_cb *cb);
static int _ymldb_distribution_init(struct ymldb_cb *cb, int flags);
static int _ymldb_distribution_send(struct ymldb_params *params);
static int _ymldb_callback_cmp(void *v1, void *v2);
static void _ymldb_callback_set(struct ymldb_params *params, struct ymldb *ydb, int deleted);
static void _ymldb_callback_run(struct ymldb_params *params);

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

#define free _free
#define malloc _malloc
#undef strdup
#define strdup _strdup

#define S10 "          "
static char *gSpace = S10 S10 S10 S10 S10 S10 S10 S10 S10 S10;

static struct ymldb *gYdb = NULL;
static cp_avltree *gYcb = NULL;
static cp_avltree *gYfd = NULL;

static unsigned int gSequence = 1;

static FILE *_ymldb_fopen_from_fd(int fd, char *rw)
{
    int dup_fd = dup(fd);
    return fdopen(dup_fd, rw);
}

static char *_ymldb_str_dump(const char *src)
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

static char *_ymldb_opcode_str(int opcode)
{
    static char opstr[128];
    opstr[0] = 0;
    strcat(opstr, "(");
    if (opcode & YMLDB_OP_SEQ)
        strcat(opstr, "seq|");
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
    if (major_key && gYcb)
    {
        struct ymldb_cb *cb = cp_avltree_get(gYcb, major_key);
        if (cb)
            return cb;
    }
    return NULL;
}

static cp_list *_ymldb_traverse_ancestors(struct ymldb *ydb, int traverse_level)
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

struct ymldb *_ymldb_get_ancestor(struct ymldb *ydb, int level)
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

static void _ymldb_dump(FILE *stream, struct ymldb *ydb, int print_level, int no_print_children);
static int _ymldb_node_dump(void *n, void *dummy)
{
    cp_avlnode *node = n;
    struct ymldb *ydb = node->value;
    FILE *stream = dummy;
    _ymldb_dump(stream, ydb, ydb->level, 0); // not print parents
    return 0;
}

static void _ymldb_dump(FILE *stream, struct ymldb *ydb, int print_level, int no_print_children)
{
    cp_list *ancestors;
    if (!ydb)
        return;
    if (print_level < ydb->level)
    { // print parents
        struct ymldb *ancestor;
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
                // _log_debug("%.*s%s:\n", (ancestor->level - 1) * 2, gSpace, ancestor->key);
                _out(stream, "%.*s%s:\n", (ancestor->level - 1) * 2, gSpace, ancestor->key);
                break;
            case YMLDB_LEAFLIST:
                // _log_debug("%.*s- %s\n", (ancestor->level - 1) * 2, gSpace, ancestor->key);
                _out(stream, "%.*s- %s\n", (ancestor->level - 1) * 2, gSpace, ancestor->key);
                break;
            case YMLDB_LEAF:
                // _log_debug("%.*s%s: %s\n", (ancestor->level - 1) * 2, gSpace, ancestor->key, ancestor->value);
                _out(stream, "%.*s%s: %s\n", (ancestor->level - 1) * 2, gSpace, ancestor->key, ancestor->value);
                break;
            }
        }
        _ymldb_traverse_free(ancestors);
    }

    if (ydb->type == YMLDB_BRANCH)
    {
        if (ydb->level != 0)
        { // not print out for top node
            // _log_debug("%.*s%s:\n", (ydb->level - 1) * 2, gSpace, ydb->key);
            _out(stream, "%.*s%s:\n", (ydb->level - 1) * 2, gSpace, ydb->key);
        }
        if (no_print_children)
            return;
        cp_avltree_callback(ydb->children, _ymldb_node_dump, stream);
    }
    else if (ydb->type == YMLDB_LEAFLIST)
    {
        // _log_debug("%.*s- %s\n", (ydb->level - 1) * 2, gSpace, ydb->key);
        _out(stream, "%.*s- %s\n", (ydb->level - 1) * 2, gSpace, _ymldb_str_dump(ydb->key));
    }
    else
    {
        // _log_debug("%.*s%s: %s\n", (ydb->level - 1) * 2, gSpace, ydb->key, ydb->value);
        _out(stream, "%.*s%s: %s\n", (ydb->level - 1) * 2, gSpace, ydb->key, _ymldb_str_dump(ydb->value));
    }
    return;
}

void _ymldb_dump_start(FILE *stream, unsigned int opcode, unsigned int sequence)
{
    // fseek(stream, 0, SEEK_SET);
    _out(stream, "# %u\n", sequence);

    // %TAG !merge! actusnetworks.com:op:
    if (opcode & YMLDB_OP_SEQ)
    {
        _out(stream, "%s %s %s%u\n", "%TAG", YMLDB_TAG_OP_SEQ, YMLDB_TAG_SEQ, sequence);
    }
    if (opcode & YMLDB_OP_MERGE)
    {
        _out(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_MERGE, YMLDB_TAG_MERGE);
    }
    else if (opcode & YMLDB_OP_DELETE)
    {
        _out(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_DELETE, YMLDB_TAG_DELETE);
    }
    else if (opcode & YMLDB_OP_GET)
    {
        _out(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_GET, YMLDB_TAG_GET);
    }
    else if (opcode & YMLDB_OP_SYNC)
    {
        _out(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_SYNC, YMLDB_TAG_SYNC);
    }

    if (opcode & YMLDB_OP_SUBSCRIBER)
    {
        _out(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_SUBSCRIBER, YMLDB_TAG_SUBSCRIBER);
    }
    else if (opcode & YMLDB_OP_PUBLISHER)
    {
        _out(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_PUBLISHER, YMLDB_TAG_PUBLISHER);
    }
    _out(stream, "---\n");
}

void _ymldb_dump_end(FILE *stream)
{
    // fflush(stream);
    _out(stream, "\n...\n\n");
}

// support readonly or writeonly.
struct ymldb_stream *ymldb_stream_alloc(size_t len)
{
    struct ymldb_stream *buf;
    if (len <= 0)
    {
        return NULL;
    }
    buf = malloc(len + sizeof(FILE *) + (sizeof(size_t) * 2) + 4);
    if (buf)
    {
        buf->stream = NULL;
        buf->buflen = len;
        buf->len = 0;
        buf->buf[0] = 0;
        buf->is_write = 0;
    }
    return buf;
}

void ymldb_stream_close(struct ymldb_stream *buf)
{
    if (buf)
    {
        if (buf->stream)
        {
            if (buf->is_write)
            {
                buf->len = ftell(buf->stream);
                buf->buf[buf->len] = 0;
            }
            fclose(buf->stream);
            buf->stream = NULL;
        }
    }
    return;
}

FILE *ymldb_stream_open(struct ymldb_stream *buf, char *rw)
{
    if (buf)
    {
        ymldb_stream_close(buf);
        if (strcmp(rw, "r") == 0)
        {
            buf->stream = fmemopen(buf->buf, buf->len, rw);
            buf->is_write = 0;
        }
        else
        { // w, w+
            buf->stream = fmemopen(buf->buf, buf->buflen, rw);
            buf->is_write = 1;
        }
        if (!buf->stream)
            return NULL;
        return buf->stream;
    }
    return NULL;
}

void ymldb_stream_free(struct ymldb_stream *buf)
{
    if (buf)
    {
        if (buf->stream)
            fclose(buf->stream);
        free(buf);
    }
}

struct ymldb_stream *ymldb_stream_alloc_and_open(size_t len, char *rw)
{
    struct ymldb_stream *buf = ymldb_stream_alloc(len);
    ymldb_stream_open(buf, rw);
    if (buf && buf->stream)
        return buf;
    ymldb_stream_free(buf);
    return NULL;
}

// Remove all subtree and data
static void _ymldb_node_free(void *vdata)
{
    struct ymldb *ydb = vdata;
    if (ydb)
    {
        if (ydb->type == YMLDB_BRANCH)
        {
            cp_avltree_destroy_custom(ydb->children, NULL, _ymldb_node_free);
        }
        else if (ydb->value)
            free(ydb->value);
        if (ydb->callback)
            free(ydb->callback);
        free(ydb->key);
        free(ydb);
    }
}

// static int _ymldb_print(void *item, void *dummy)
// {
//     if (dummy)
//         _log_debug(" -- %s=%s\n", (char *)dummy, (char *)item);
//     else
//         _log_debug(" -- %s\n", (char *)item);
//     return 0;
// }

int _ymldb_print_level(struct ymldb *last_ydb, struct ymldb *cur_ydb)
{
    int print_level = 0;
    struct ymldb *ancestor1;
    struct ymldb *ancestor2;
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

void _ymldb_node_merge_reply(struct ymldb_params *params, struct ymldb *ydb)
{
    int flushed;
    int print_level = 0;
    if (!params || !ydb)
        return;
    params->no_change = 0;
    if (params->last_ydb)
        print_level = _ymldb_print_level(params->last_ydb, ydb);
    _ymldb_dump(params->streambuffer->stream, ydb, print_level, 0);
    params->last_ydb = ydb;
    flushed = _ymldb_params_streambuffer_flush(params, 0);
    if (flushed)
        _ymldb_params_streambuffer_init(params);
    _ymldb_callback_set(params, ydb, 0);
    return;
}

struct ymldb *_ymldb_node_merge(struct ymldb_params *params, struct ymldb *parent,
                                ymldb_type_t type, char *key, char *value)
{
    struct ymldb *ydb = NULL;
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
                cp_avltree_delete(parent->children, ydb->key);
                _ymldb_node_free(ydb);
                goto new_ydb;
            }

            if (ydb->type == YMLDB_LEAF)
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

new_ydb:
    ydb = malloc(sizeof(struct ymldb));
    if (!ydb)
        goto free_ydb;
    memset(ydb, 0, sizeof(struct ymldb));

    ykey = strdup(key);
    if (!ykey)
        goto free_ydb;

    ydb->key = ykey;
    ydb->type = type;
    ydb->callback = NULL;

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
        if (type != YMLDB_BRANCH && ydb->children)
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

void _ymldb_node_delete(struct ymldb_params *params, struct ymldb *parent, char *key)
{
    int flushed;
    int print_level = 0;
    if (!params || !parent || !key)
        return;

    struct ymldb *ydb = NULL;
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
    if (params->last_ydb)
        print_level = _ymldb_print_level(params->last_ydb, ydb);
    _ymldb_dump(params->streambuffer->stream, ydb, print_level, 1);
    params->last_ydb = parent; // parent should be saved because of the ydb will be removed.
    flushed = _ymldb_params_streambuffer_flush(params, 0);
    if (flushed)
        _ymldb_params_streambuffer_init(params);
    _ymldb_callback_set(params, ydb, 1);
    ydb = cp_avltree_delete(parent->children, key);
    _ymldb_node_free(ydb);
    return;
}

void _ymldb_node_get(struct ymldb_params *params, struct ymldb *parent, char *key)
{
    int flushed;
    int print_level = 0;
    struct ymldb *ydb = NULL;
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
    ydb = cp_avltree_get(parent->children, key);
    if (!ydb)
    {
        _log_error("'%s' doesn't exists\n", key);
        params->res--;
        return;
    }

    if (params->last_ydb)
        print_level = _ymldb_print_level(params->last_ydb, ydb);
    _ymldb_dump(params->streambuffer->stream, ydb, print_level, 0);
    params->last_ydb = ydb;
    flushed = _ymldb_params_streambuffer_flush(params, 0);
    if (flushed)
        _ymldb_params_streambuffer_init(params);
    return;
}

int _ymldb_internal_merge(struct ymldb_params *params, struct ymldb *p_ydb, int index, int p_index)
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
                // _log_debug("## key %s value %s\n", key, value);
                _ymldb_node_merge(params, p_ydb, YMLDB_LEAF, key, value);
            }
            else
            { // not leaf
                struct ymldb *ydb = NULL;
                // _log_debug("key %s\n", key);
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

int _ymldb_internal_delete(struct ymldb_params *params, struct ymldb *p_ydb, int index, int p_index)
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
            _log_debug("key %s\n", key);
            if (value_node->type == YAML_SCALAR_NODE)
            {
                if (value[0] > 0)
                {
                    struct ymldb *ydb = NULL;
                    ydb = cp_avltree_get(p_ydb->children, key);
                    _ymldb_node_delete(params, ydb, value);
                }
                else
                    _ymldb_node_delete(params, p_ydb, key);
            }
            else
            { // not leaf
                struct ymldb *ydb = NULL;
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
        _log_debug("scalar key %s, value -\n", key);
        _ymldb_node_delete(params, p_ydb, key);
    }
    break;
    case YAML_NO_NODE:
    default:
        break;
    }
    return 0;
}

int _ymldb_internal_get(struct ymldb_params *params, struct ymldb *p_ydb, int index, int p_index)
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
            if (node->type == YAML_SCALAR_NODE)
            {
                _ymldb_node_get(params, p_ydb, key);
            }
            else
            {
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
            _log_debug("key %s\n", key);
            if (value_node->type == YAML_SCALAR_NODE)
            {
                if (value[0] > 0)
                {
                    struct ymldb *ydb = NULL;
                    ydb = cp_avltree_get(p_ydb->children, key);
                    _ymldb_node_get(params, ydb, value);
                }
                else
                    _ymldb_node_get(params, p_ydb, key);
            }
            else
            { // not leaf
                struct ymldb *ydb = NULL;
                // if(p_ydb->type == YMLDB_BRANCH) {
                ydb = cp_avltree_get(p_ydb->children, key);
                _ymldb_internal_get(params, ydb, pair->value, index);
                // }
            }
        }
    }
    break;
    case YAML_SCALAR_NODE:
    { // It is only used for single key inserted..
        char *key;
        key = (char *)node->data.scalar.value;
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
            if (node->type == YAML_SCALAR_NODE)
            {
                _out(stream, "%.*s- %s\n", level * 2, gSpace, key);
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
            // _log_debug("key %s\n", key);
            if (value_node->type == YAML_SCALAR_NODE)
            {
                _out(stream, "%.*s%s: %s\n", level * 2, gSpace, key, value);
            }
            else
            { // not leaf
                _out(stream, "%.*s%s:\n", level * 2, gSpace, key);
                _ymldb_internal_relay(params, level + 1, pair->value, index);
            }
        }
    }
    break;
    case YAML_SCALAR_NODE:
    { // It is only used for single key inserted..
        char *key;
        key = (char *)node->data.scalar.value;
        _out(stream, "%.*s%s\n", level * 2, gSpace, key);
    }
    break;
    case YAML_NO_NODE:
    default:
        break;
    }
    return 0;
}

int _ymldb_params_opcode_extract(struct ymldb_params *params)
{
    unsigned int opcode = 0;
    unsigned int sequence;
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
                opcode = opcode | YMLDB_OP_SEQ;
                sscanf((char *)tag->prefix, YMLDB_TAG_SEQ "%u", &sequence);
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

static int _ymldb_state_machine(struct ymldb_params *params)
{
    int no_reply = 1;
    int no_change = 1;
    int ignore_or_relay = 0;
    unsigned int out_opcode = 0;
    unsigned int out_sequence = 0;
    unsigned int in_opcode = params->in.opcode;
    unsigned int in_sequence = params->in.sequence;
    unsigned int flags = params->cb->flags;

    if (!(in_opcode & YMLDB_OP_ACTION))
    {
        // do nothing if no action.
        ignore_or_relay = 1;
    }
    if (flags & YMLDB_FLAG_PUBLISHER)
    {
        if (in_opcode & YMLDB_OP_SUBSCRIBER)
        {
            if (in_opcode & (YMLDB_OP_MERGE | YMLDB_OP_DELETE))
                ignore_or_relay = 1;
            else // YMLDB_OP_GET, YMLDB_OP_SYNC
            {
                out_opcode = YMLDB_OP_PUBLISHER;
                out_opcode |= (in_opcode & (YMLDB_OP_GET | YMLDB_OP_SYNC));
                no_reply = 0;
            }
        }
        else if (in_opcode & YMLDB_OP_PUBLISHER)
            ignore_or_relay = 1;
        else
        {
            out_opcode = YMLDB_OP_PUBLISHER;
            out_opcode |= (in_opcode & YMLDB_OP_ACTION);
            no_reply = 0;
            if (in_opcode & YMLDB_OP_GET)
                no_reply = 1;
            if (flags & YMLDB_FLAG_NOSYNC)
                no_reply = 1;
        }
    }
    else if (flags & YMLDB_FLAG_SUBSCRIBER)
    {
        if (in_opcode & YMLDB_OP_SUBSCRIBER)
            ignore_or_relay = 1;
        else if (in_opcode & YMLDB_OP_PUBLISHER)
        {
            if (in_opcode & YMLDB_OP_GET)
                ignore_or_relay = 1;
            else // YMLDB_OP_MERGE, YMLDB_OP_DELETE, YMLDB_OP_SYNC
            {
                out_opcode = YMLDB_OP_SUBSCRIBER;
                out_opcode |= (in_opcode & (YMLDB_OP_MERGE | YMLDB_OP_DELETE | YMLDB_OP_SYNC));
            }
        }
        else
        {
            if (in_opcode & (YMLDB_OP_MERGE | YMLDB_OP_DELETE))
                ignore_or_relay = 1;
            else // YMLDB_OP_GET, YMLDB_OP_SYNC
            {
                out_opcode = YMLDB_OP_SUBSCRIBER;
                out_opcode |= (in_opcode & (YMLDB_OP_GET | YMLDB_OP_SYNC));
                if (in_opcode & YMLDB_OP_SYNC)
                {
                    no_reply = 0;
                    ignore_or_relay = 2;
                }
            }
        }
    }
    else
    { // ymldb for local user
        if (in_opcode & YMLDB_OP_SUBSCRIBER)
            ignore_or_relay = 1;
        else if (in_opcode & YMLDB_OP_PUBLISHER)
            ignore_or_relay = 1;
        else
        {
            out_opcode |= (in_opcode & YMLDB_OP_ACTION);
            no_reply = 1;
            if (in_opcode & YMLDB_OP_SYNC)
                ignore_or_relay = 1;
        }
    }
    out_opcode |= YMLDB_OP_SEQ;
    if (in_opcode & YMLDB_OP_SEQ)
    {
        out_sequence = in_sequence;
    }
    else
    {
        out_sequence = gSequence;
        gSequence++;
    }
    params->out.opcode = out_opcode;
    params->out.sequence = out_sequence;
    params->no_reply = no_reply;
    params->no_change = no_change;
    return ignore_or_relay;
}

static void _ymldb_params_free(struct ymldb_params *params)
{
    yaml_parser_t *parser;
    yaml_document_t *document;
    struct ymldb_stream *streambuffer;
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
        ymldb_stream_free(streambuffer);
    }
    if (params->callbacks)
    {
        cp_avltree_destroy(params->callbacks);
    }
    free(params);
    return;
}

static struct ymldb_params *_ymldb_params_alloc(struct ymldb_cb *cb, FILE *instream, FILE *outstream)
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
    params->streambuffer = ymldb_stream_alloc_and_open(YMLDB_STREAM_BUF_SIZE, "w+");
    if (!params->streambuffer)
    {
        _log_error("streambuffer alloc failed.\n");
        goto failed;
    }
    params->callbacks = NULL;
    params->res = 0;
    params->cb = cb;
    return params;
failed:
    ymldb_stream_free(params->streambuffer);
    _ymldb_params_free(params);
    return NULL;
}

static int _ymldb_params_document_load(struct ymldb_params *params)
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

static int _ymldb_params_streambuffer_init(struct ymldb_params *params)
{
    params->last_ydb = NULL;
    _ymldb_dump_start(params->streambuffer->stream, params->out.opcode, params->out.sequence);
    return 0;
}

// Return 1 if reply.stream is flushed, otherwise 0.
static int _ymldb_params_streambuffer_flush(struct ymldb_params *params, int forced)
{
    struct ymldb_stream *streambuffer = params->streambuffer;
    if (forced)
        goto flushing;
    else if (ftell(streambuffer->stream) >= YMLDB_STREAM_THRESHOLD)
        goto flushing;
    return 0;

flushing:
    _ymldb_dump_end(streambuffer->stream);
    // write the stream to streambuffer->buf.
    fflush(streambuffer->stream);
    streambuffer->buf[ftell(streambuffer->stream)] = 0; // end of string.
    _log_debug("\n###### START\n%s", streambuffer->buf);
    _log_debug("\n###### END\n");

    if (params->out.stream)
    {
        fputs(streambuffer->buf, params->out.stream);
        fflush(params->out.stream);
    }

    if (params->cb->flags & YMLDB_FLAG_CONN)
        _ymldb_distribution_send(params);

    fseek(streambuffer->stream, 0, SEEK_SET);
    _log_debug("ftell()=%d\n", (int)ftell(streambuffer->stream));
    return 1;
}

int _ymldb_run(struct ymldb_cb *cb, FILE *instream, FILE *outstream)
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
    params = _ymldb_params_alloc(cb, instream, outstream);
    if (!params)
    {
        return -1;
    }
    _log_debug("major_key %s\n", cb->key);
    _log_debug(">>>\n");
    while (!done)
    {
        yaml_node_t *yroot = NULL;
        /* Get the next ymldb document. */
        res = _ymldb_params_document_load(params);
        if (res < 0)
        {
            params->res--;
            break;
        }
        _ymldb_params_opcode_extract(params);
        yroot = yaml_document_get_root_node(&params->document);
        if (yroot)
        {
            int ignore_or_relay;
            _log_debug("out %dth\n", params->out.sequence);
            _log_debug("in %s\n", _ymldb_opcode_str(params->in.opcode));
            ignore_or_relay = _ymldb_state_machine(params);
            if (ignore_or_relay == 1)
            {
                _log_debug("in %uth %s\n", params->in.sequence, "ignored ...");
                params->res--;
                continue;
            }
            _log_debug("out %dth\n", params->out.sequence);
            _log_debug("out %s\n", _ymldb_opcode_str(params->out.opcode));

            _ymldb_params_streambuffer_init(params);
            if (params->out.opcode & YMLDB_OP_MERGE)
                _ymldb_internal_merge(params, gYdb, 1, 1);
            else if (params->out.opcode & YMLDB_OP_DELETE)
                _ymldb_internal_delete(params, gYdb, 1, 1);
            else if (params->out.opcode & YMLDB_OP_GET)
                _ymldb_internal_get(params, gYdb, 1, 1);
            else if (params->out.opcode & YMLDB_OP_SYNC)
            {
                if (ignore_or_relay == 2)
                    _ymldb_internal_relay(params, 0, 1, 1);
                else if (params->in.opcode & YMLDB_OP_PUBLISHER)
                    _ymldb_internal_merge(params, gYdb, 1, 1);
                else
                    _ymldb_internal_get(params, gYdb, 1, 1);
            }
            _ymldb_params_streambuffer_flush(params, 1); // forced flush!
            _log_debug("result: %s\n", params->res < 0 ? "failed" : "ok");
        }
        else
        {
            done = 1;
        }
    }
    _log_debug("<<<\n");
    res = params->res;
    _ymldb_callback_run(params);
    _ymldb_params_free(params);
    return res;
}

int ymldb_run(char *major_key, FILE *instream, FILE *outstream)
{
    int res;
    struct ymldb_cb *cb;
    _log_empty();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb key found.\n");
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
    _log_empty();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb key found.\n");
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

static int _ymldb_yfd_cmp(void *v1, void *v2)
{
     // struct ymldb_cb *cb1;
     // struct ymldb_cb *cb2;
    return v1 - v2;
}

int ymldb_create(char *major_key, unsigned int flags)
{
    struct ymldb_cb *cb;
    _log_empty();
    if (!major_key)
    {
        _log_error("no major key\n");
        return -1;
    }
    _log_debug("\n");
    // init top
    if (!gYdb)
    {
        gYdb = _ymldb_node_merge(NULL, NULL, YMLDB_BRANCH, "top", NULL);
        if (!gYdb)
        {
            _log_error("gYdb failed.\n");
            return -1;
        }
    }

    if (!gYcb)
    {
        gYcb = cp_avltree_create((cp_compare_fn)strcmp);
        if (!gYcb)
        {
            _log_error("gYcb failed.\n");
            return -1;
        }
    }

    if(!gYfd)
    {
        gYfd = cp_avltree_create((cp_compare_fn)_ymldb_yfd_cmp);
        if (!gYfd)
        {
            _log_error("gYfd failed.\n");
            return -1;
        }
    }

    if (cp_avltree_get(gYdb->children, major_key))
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
        cb->fd_subscriber[i] = -1;

    cb->ydb = _ymldb_node_merge(NULL, gYdb, YMLDB_BRANCH, major_key, NULL);
    if (!cb->ydb)
    {
        _log_error("init failed.\n");

        free(cb->key);
        free(cb);
        return -1;
    }

    _log_debug("major_key %s\n", major_key);

    if (flags & YMLDB_FLAG_NOSYNC)
        cb->flags |= YMLDB_FLAG_NOSYNC;
    if (flags & YMLDB_FLAG_PUBLISHER || flags & YMLDB_FLAG_SUBSCRIBER)
        _ymldb_distribution_init(cb, flags);

    cp_avltree_insert(gYcb, cb->key, cb);
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
        struct ymldb *ydb = cb->ydb;
        if (ydb->parent)
        {
            cp_avltree_delete(cb->ydb->parent->children, ydb->key);
        }
        _ymldb_node_free(ydb);
    }
    _ymldb_distribution_deinit(cb);
    _log_debug("\n");
    if (cb->key)
        free(cb->key);
    _log_debug("\n");
    free(cb);
}

void ymldb_destroy(char *major_key)
{
    struct ymldb_cb *cb;
    _log_empty();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb key found.\n");
        return;
    }
    _log_debug("major_key %s\n", major_key);

    cp_avltree_delete(gYcb, cb->key);
    _ymldb_destroy(cb);
    if (cp_avltree_count(gYcb) <= 0)
    {
        _ymldb_node_free(gYdb);
        cp_avltree_destroy(gYcb);
        _log_debug("all destroyed ...\n");
        gYcb = NULL;
        gYdb = NULL;
    }
}

void ymldb_destroy_all()
{
    _log_empty();
    _log_debug("\n");
    if (gYcb)
    {
        cp_avltree_destroy_custom(gYcb, NULL, _ymldb_destroy);
        _ymldb_node_free(gYdb);
        _log_debug("all destroyed ...\n");
        gYcb = NULL;
        gYdb = NULL;
    }
}

static int _ymldb_distribution_deinit(struct ymldb_cb *cb)
{
    if (!cb)
    {
        _log_error("no cb\n");
        return -1;
    }

    _log_debug("deinit conn\n");

    if (cb->fd_publisher >= 0)
    {
        close(cb->fd_publisher);
        cb->fd_publisher = -1;
    }
    if (cb->flags & YMLDB_FLAG_PUBLISHER)
    {
        int i;
        for (i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
        {
            if (cb->fd_subscriber[i] >= 0)
            {
                close(cb->fd_subscriber[i]);
                cb->fd_subscriber[i] = -1;
            }
        }
    }
    cb->flags = cb->flags & (~YMLDB_FLAG_CONN);
    cb->flags = cb->flags & (~YMLDB_FLAG_PUBLISHER);
    cb->flags = cb->flags & (~YMLDB_FLAG_SUBSCRIBER);
    cb->flags = cb->flags & (~YMLDB_FLAG_RECONNECT);
    return 0;
}

static int _ymldb_distribution_init(struct ymldb_cb *cb, int flags)
{
    int fd;
    char socketpath[128];
    struct sockaddr_un addr;
    _log_debug("\n");
    if (cb->flags & YMLDB_FLAG_CONN)
    {
        _ymldb_distribution_deinit(cb);
    }
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        _log_error("socket failed (%s)\n", strerror(errno));
        cb->flags |= YMLDB_FLAG_RECONNECT;
        return -1;
    }
    snprintf(socketpath, sizeof(socketpath), YMLDB_UNIXSOCK_PATH, cb->key);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketpath, sizeof(addr.sun_path) - 1);
    addr.sun_path[0] = 0;
    cb->fd_publisher = fd;
    if (flags & YMLDB_FLAG_PUBLISHER)
    { // PUBLISHER
        cb->flags |= YMLDB_FLAG_PUBLISHER;
        if (flags & YMLDB_FLAG_NOSYNC)
            cb->flags |= YMLDB_FLAG_NOSYNC;
        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            _log_error("bind failed (%s).\n", strerror(errno));
            cb->flags |= YMLDB_FLAG_RECONNECT;
            return -1;
        }
        if (listen(fd, YMLDB_SUBSCRIBER_MAX) < 0)
        {
            _log_error("listen failed (%s).\n", strerror(errno));
            cb->flags |= YMLDB_FLAG_RECONNECT;
            return -1;
        }
    }
    else if (flags & YMLDB_FLAG_SUBSCRIBER)
    { // SUBSCRIBER
        cb->flags |= YMLDB_FLAG_SUBSCRIBER;
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        {
            _log_error("connect failed (%s).\n", strerror(errno));
            cb->flags |= YMLDB_FLAG_RECONNECT;
            return -1;
        }
    }
    return fd;
}

static int _ymldb_distribution_set(void *n, void *dummy)
{
    cp_avlnode *node = n;
    struct ymldb_cb *cb = node->value;
    struct ymldb_distribution *yd = dummy;
    int max = yd->max;
    fd_set *set = yd->set;
    _log_debug("\n");
    if (cb->flags & YMLDB_FLAG_CONN)
    {
        if (cb->flags & YMLDB_FLAG_RECONNECT)
        {
            _log_debug("RECONN\n");
            return 0;
        }
        if (cb->fd_publisher >= 0)
        {
            _log_debug("cb %s fd_publisher %d\n", cb->key, cb->fd_publisher);
            FD_SET(cb->fd_publisher, set);
            max = cb->fd_publisher > max ? cb->fd_publisher : max;
        }
        if (cb->flags & YMLDB_FLAG_PUBLISHER)
        {
            int i;
            for (i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
            {
                if (cb->fd_subscriber[i] >= 0)
                {
                    _log_debug("cb %s fd_subscriber[%d] %d\n", cb->key, i, cb->fd_subscriber[i]);
                    FD_SET(cb->fd_subscriber[i], set);
                    max = cb->fd_subscriber[i] > max ? cb->fd_subscriber[i] : max;
                }
            }
        }
    }
    _log_debug("max fd %d\n", max);
    yd->max = max;
    return 0;
}

static int _ymldb_distribution_recv_internal(struct ymldb_cb *cb, FILE *outstream, fd_set *set)
{
    _log_debug("\n");
    if (!(cb->flags & YMLDB_FLAG_CONN))
    {
        _log_error("not a subscriber or publisher\n");
        return -1;
    }
    if (cb->flags & YMLDB_FLAG_RECONNECT)
    {
        return _ymldb_distribution_init(cb, cb->flags);
    }
    if (cb->fd_publisher >= 0)
    {
        if (FD_ISSET(cb->fd_publisher, set))
        {
            if (cb->flags & YMLDB_FLAG_PUBLISHER)
            { // PUBLISHER
                int i, fd;
                FD_CLR(cb->fd_publisher, set);
                fd = accept(cb->fd_publisher, NULL, NULL);
                if (fd < 0)
                {
                    _log_error("accept failed (%s)\n", strerror(errno));
                    return -1;
                }
                for (i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
                {
                    if (cb->fd_subscriber[i] < 0)
                    {
                        cb->fd_subscriber[i] = fd;
                        _log_debug("subscriber (fd %d) added..\n", fd);
                        if (!(cb->flags & YMLDB_FLAG_NOSYNC))
                            ymldb_sync(cb->key);
                        break;
                    }
                }
                if (i >= YMLDB_SUBSCRIBER_MAX)
                {
                    _log_error("subscription over..\n");
                }
            }
            else if (cb->flags & YMLDB_FLAG_SUBSCRIBER)
            { // SUBSCRIBER
                FD_CLR(cb->fd_publisher, set);
                struct ymldb_stream *input;
                input = ymldb_stream_alloc(YMLDB_STREAM_BUF_SIZE);
                if (!input)
                {
                    _log_error("fail to open ymldb stream\n");
                    return -1;
                }
                input->len = read(cb->fd_publisher, input->buf, YMLDB_STREAM_BUF_SIZE);
                if (input->len <= 0)
                {
                    cb->flags |= YMLDB_FLAG_RECONNECT;
                    if (input->len < 0)
                        _log_error("fd %d read failed (%s)\n", cb->fd_publisher, strerror(errno));
                    else
                        _log_error("fd %d closed (EOF)\n", cb->fd_publisher);
                    _log_debug("retry to connect to publisher.\n");
                    ymldb_stream_free(input);
                    return -1;
                }
                input->buf[input->len] = 0;
                _log_debug("len=%zd buf=\n%s\n", input->len, input->buf);
                ymldb_stream_open(input, "r");
                _ymldb_run(cb, input->stream, outstream);
                ymldb_stream_free(input);
            }
        }
    }
    if (cb->flags & YMLDB_FLAG_PUBLISHER)
    {
        int i;
        struct ymldb_stream *input = NULL;
        for (i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
        {
            if (cb->fd_subscriber[i] < 0)
                continue;
            if (FD_ISSET(cb->fd_subscriber[i], set))
            {
                FD_CLR(cb->fd_subscriber[i], set);
                if (!input)
                {
                    input = ymldb_stream_alloc(YMLDB_STREAM_BUF_SIZE);
                    if (!input)
                    {
                        _log_error("fail to open ymldb stream\n");
                        return -1;
                    }
                }
                _log_debug("fd %d\n", cb->fd_subscriber[i]);
                input->len = read(cb->fd_subscriber[i], input->buf, YMLDB_STREAM_BUF_SIZE);
                if (input->len <= 0)
                {
                    if (input->len < 0)
                        _log_error("subscriber (fd %d) read failed (%s)\n", cb->fd_subscriber[i], strerror(errno));
                    else
                        _log_error("subscriber (fd %d) closed (EOF)\n", cb->fd_subscriber[i]);
                    _log_debug("close the conn (%d).\n", cb->fd_subscriber[i]);
                    close(cb->fd_subscriber[i]);
                    cb->fd_subscriber[i] = -1;
                    continue;
                }
                input->buf[input->len] = 0;
                _log_debug("len=%zd buf=\n%s\n", input->len, input->buf);
                ymldb_stream_open(input, "r");
                _ymldb_run(cb, input->stream, outstream);
                ymldb_stream_close(input);
            }
        }
        ymldb_stream_free(input);
    }
    return 0;
}

static int _ymldb_distribution_recv(void *n, void *dummy)
{
    cp_avlnode *node = n;
    struct ymldb_cb *cb = node->value;
    struct ymldb_distribution *yd = dummy;
    return _ymldb_distribution_recv_internal(cb, yd->stream, yd->set);
}

// available for subscriber
static int _ymldb_sync_wait(struct ymldb_cb *cb, FILE *outstream, int sec, int usec)
{
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
        int res = _ymldb_distribution_init(cb, cb->flags);
        if (res < 0)
            return res;
    }
    if (cb->fd_publisher >= 0)
    {
        int res;
        fd_set set;
        struct timeval tv;
        tv.tv_sec = sec;
        tv.tv_usec = usec;
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
            _log_error("fd %d timeout\n", cb->fd_publisher);
            return -1;
        }
        _ymldb_distribution_recv_internal(cb, outstream, &set);
    }
    return 0;
}

static int _ymldb_distribution_send(struct ymldb_params *params)
{
    int res = 0;
    int sent = 0;
    int retry = 0;
    struct ymldb_cb *cb = params->cb;
    struct ymldb_stream *streambuffer = params->streambuffer;
    if (params->no_reply)
    {
        _log_debug("no reply\n");
        return 0;
    }
    if (params->out.opcode & (YMLDB_OP_MERGE | YMLDB_OP_DELETE))
    {
        if (params->no_change)
        {
            _log_debug("discard due to no change in ymldb\n");
            return 0;
        }
    }
    if (cb->flags & YMLDB_FLAG_RECONNECT)
    {
        _log_error("reconn '%s'.\n", cb->key);
        return -1;
    }
    if (cb->fd_publisher < 0)
    {
        cb->flags |= YMLDB_FLAG_RECONNECT;
        _log_error("reconn '%s'\n", cb->key);
        return -1;
    }
    if (cb->flags & YMLDB_FLAG_SUBSCRIBER)
    {
    subscriber_rewrite:
        res = write(cb->fd_publisher, streambuffer->buf + sent, ftell(streambuffer->stream) - sent);
        if (res < 0)
        {
            cb->flags |= YMLDB_FLAG_RECONNECT;
            _log_error("fd %d send failed (%s)\n",
                       cb->fd_publisher, strerror(errno));
            return -1;
        }
        sent = res + sent;
        if (sent < ftell(streambuffer->stream) && retry < 3)
        {
            retry++;
            goto subscriber_rewrite;
        }
    }
    else if (cb->flags & YMLDB_FLAG_PUBLISHER)
    {
        int i;
        for (i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
        {
            if (cb->fd_subscriber[i] >= 0)
            {
                sent = 0;
                retry = 0;
            publisher_rewrite:
                res = write(cb->fd_subscriber[i], streambuffer->buf + sent, ftell(streambuffer->stream) - sent);
                if (res < 0)
                {
                    _log_error("fd %d send failed (%s)\n",
                               cb->fd_subscriber[i], strerror(errno));
                    close(cb->fd_subscriber[i]);
                    cb->fd_subscriber[i] = -1;
                    continue;
                }
                sent = res + sent;
                if (sent < ftell(streambuffer->stream) && retry < 3)
                {
                    retry++;
                    goto publisher_rewrite;
                }
            }
        }
    }
    return 0;
}

int ymldb_distribution_get_publisher_fd(char *major_key, int *fd)
{
    struct ymldb_cb *cb;
    _log_empty();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb key found.\n");
        return -1;
    }
    if(!fd) {
        _log_error("no *fd configured.\n");
        return -1;
    }
    *fd = cb->fd_publisher;
    return 0;
}

int ymldb_distribution_get_subscriber_fds(char *major_key, int **fds, int* fds_count)
{
    struct ymldb_cb *cb;
    _log_empty();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb key found.\n");
        return -1;
    }
    if(!fds || !fds_count) {
        _log_error("no **fds or *fds_count configured.\n");
        return -1;
    }

    *fds = cb->fd_subscriber;
    *fds_count = YMLDB_SUBSCRIBER_MAX;
    return 0;
}


int _ymldb_push(FILE *outstream, unsigned int opcode, char *major_key, char *format, ...)
{
    int res;
    struct ymldb_stream *input;
    struct ymldb_cb *cb;
    _log_empty();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb key found.\n");
        return -1;
    }
    if (opcode == 0)
    {
        _log_error("opcode\n");
        return -1;
    }
    _log_debug("\n");
    input = ymldb_stream_alloc_and_open(512, "w");
    if (!input)
    {
        _log_error("fail to alloc ymldb stream\n");
        return -1;
    }
    // write ymldb data to the streambuf
    _ymldb_dump_start(input->stream, opcode, 0);
    va_list args;
    va_start(args, format);
    vfprintf(input->stream, format, args);
    va_end(args);
    _ymldb_dump_end(input->stream);

    ymldb_stream_open(input, "r");
    if (!input->stream)
    {
        _log_error("fail to open ymldb stream");
        ymldb_stream_free(input);
        return -1;
    }
    _log_debug("input->len=%zd buf=\n%s\n", input->len, input->buf);
    res = _ymldb_run(cb, input->stream, outstream);
    _log_debug("result: %s\n", res < 0 ? "failed" : "ok");
    ymldb_stream_free(input);
    return res;
}

int ymldb_push(char *major_key, char *format, ...)
{
    int res;
    struct ymldb_stream *input;
    unsigned int opcode = YMLDB_OP_MERGE;
    struct ymldb_cb *cb;
    _log_empty();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb key found.\n");
        return -1;
    }
    _log_debug("\n");
    input = ymldb_stream_alloc_and_open(512, "w");
    if (!input)
    {
        _log_error("fail to open ymldb stream\n");
        return -1;
    }
    // write ymldb data to the streambuf
    _ymldb_dump_start(input->stream, opcode, 0);
    va_list args;
    va_start(args, format);
    vfprintf(input->stream, format, args);
    va_end(args);
    _ymldb_dump_end(input->stream);

    ymldb_stream_open(input, "r");
    if (!input->stream)
    {
        _log_error("fail to open ymldb stream");
        ymldb_stream_free(input);
        return -1;
    }
    _log_debug("input->len=%zd buf=\n%s\n", input->len, input->buf);
    res = _ymldb_run(cb, input->stream, NULL);
    _log_debug("result: %s\n", res < 0 ? "failed" : "ok");
    ymldb_stream_free(input);
    return res;
}

void ymldb_remove_specifiers(FILE *dest, char *src)
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
    struct ymldb_stream *input = NULL;
    struct ymldb_stream *output = NULL;
    struct ymldb_cb *cb;
    _log_empty();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb key found.\n");
        return -1;
    }
    _log_debug("\n");
    input = ymldb_stream_alloc_and_open(512, "w");
    if (!input)
    {
        _log_error("fail to open ymldb stream\n");
        goto failed;
    }
    output = ymldb_stream_alloc_and_open(512, "w");
    if (!output)
    {
        _log_error("fail to open ymldb stream\n");
        goto failed;
    }

    _ymldb_dump_start(input->stream, opcode, 0);
    ymldb_remove_specifiers(input->stream, format);
    _ymldb_dump_end(input->stream);

    ymldb_stream_open(input, "r");
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
        // fflush(output->stream);
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
    ymldb_stream_free(input);
    ymldb_stream_free(output);
    return res;
}

// write a key and value
int _ymldb_write(FILE *outstream, unsigned int opcode, char *major_key, ...)
{
    int res;
    int level = 0;
    struct ymldb_stream *input;
    struct ymldb_cb *cb;
    _log_empty();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb key found.\n");
        return -1;
    }
    if (opcode == 0)
    {
        _log_error("opcode\n");
        return -1;
    }
    _log_debug("major_key %s\n", cb->key);
    _log_debug("opcode %s\n", _ymldb_opcode_str(opcode));
    input = ymldb_stream_alloc_and_open(256, "w");
    if (!input)
    {
        _log_error("fail to open ymldb stream\n");
        return -1;
    }

    _ymldb_dump_start(input->stream, opcode, 0);
    fprintf(input->stream, "%.*s%s:\n", level * 2, gSpace, major_key);
    level++;

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
            fprintf(input->stream, "%.*s%s\n", level * 2, gSpace, cur_token);
            break;
        }
        fprintf(input->stream, "%.*s%s:\n", level * 2, gSpace, cur_token);
        cur_token = next_token;
        level++;
    };
    va_end(args);
    _ymldb_dump_end(input->stream);

    ymldb_stream_open(input, "r");
    if (!input->stream)
    {
        _log_error("fail to open ymldb stream");
        ymldb_stream_free(input);
        return -1;
    }
    _log_debug("len=%zd buf=\n%s\n", input->len, input->buf);
    res = _ymldb_run(cb, input->stream, outstream);
    _log_debug("result: %s\n", res < 0 ? "failed" : "ok");
    ymldb_stream_free(input);
    if (res >= 0)
    {
        if ((opcode & YMLDB_OP_SYNC) && cb->flags & YMLDB_FLAG_SUBSCRIBER)
        {
            res = _ymldb_sync_wait(cb, outstream, 1, 0);
        }
    }
    return res;
}

// read a value by a key.
char *_ymldb_read(char *major_key, ...)
{ // directly access to ymldb.
    struct ymldb *ydb;
    struct ymldb_cb *cb;
    _log_empty();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb key found.\n");
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

void ymldb_dump_all(FILE *outstream)
{
    if (!outstream)
        return;
    fprintf(outstream, "\n [Current ymldb tree]\n\n");
    _ymldb_dump(outstream, gYdb, 0, 0);
    fprintf(outstream, "\n  @@ g_alloc_count %d @@\n\n", g_alloc_count);
}

int ymldb_distribution_deinit(char *major_key)
{
    struct ymldb_cb *cb;
    _log_empty();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb key found.\n");
        return -1;
    }
    return _ymldb_distribution_deinit(cb);
}

int ymldb_distribution_init(char *major_key, int flags)
{
    struct ymldb_cb *cb;
    _log_empty();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb key found.\n");
        return -1;
    }
    return _ymldb_distribution_init(cb, flags);
}

int ymldb_distribution_set(fd_set *set)
{
    struct ymldb_distribution yd;
    yd.set = set;
    yd.max = 0;
    _log_empty();
    if (!set)
    {
        _log_error("no fd_set\n");
        return -1;
    }
    if (gYcb)
        cp_avltree_callback(gYcb, _ymldb_distribution_set, &yd);
    return yd.max;
}

int ymldb_distribution_recv_and_dump(FILE *outstream, fd_set *set)
{
    struct ymldb_distribution yd;
    yd.set = set;
    yd.max = 0;
    yd.stream = outstream;
    _log_empty();
    if (!set)
    {
        _log_error("no fd_set\n");
        return -1;
    }
    if (gYcb)
        cp_avltree_callback(gYcb, _ymldb_distribution_recv, &yd);
    return 0;
}

int ymldb_distribution_recv(fd_set *set)
{
    return ymldb_distribution_recv_and_dump(NULL, set);
}

int ymldb_distribution_add(char *major_key, int subscriber_fd)
{
    struct ymldb_cb *cb;
    _log_empty();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb key found.\n");
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
            _log_debug("subscriber (fd %d) added..\n", subscriber_fd);
            if (!(cb->flags & YMLDB_FLAG_NOSYNC))
                ymldb_sync(cb->key);
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
    _log_empty();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb key found.\n");
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
            _log_debug("subscriber (fd %d) deleted..\n", subscriber_fd);
            break;
        }
    }
    return 0;
}

int _ymldb_callback_register(ymldb_callback_fn usr_func, void *usr_data, char *major_key, ...)
{
    struct ymldb *ydb;
    struct ymldb *p_ydb;
    struct ymldb_cb *cb;
    _log_empty();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb key found.\n");
        return -1;
    }
    if (!usr_func)
    {
        _log_error("no usr_func\n");
        return -1;
    }
    p_ydb = cb->ydb;
    _log_debug("key %s\n", p_ydb->key);
    char *cur_token;
    va_list args;
    va_start(args, major_key);
    cur_token = va_arg(args, char *);
    while (cur_token != NULL)
    {
        _log_debug("key %s\n", cur_token);
        if (p_ydb->type != YMLDB_BRANCH)
        {
            _log_error("usr_func is unable to be registered to ymldb leaf.!\n");
            return -1;
        }
        ydb = cp_avltree_get(p_ydb->children, cur_token);
        if (!ydb)
        {
            ydb = _ymldb_node_merge(NULL, p_ydb, YMLDB_BRANCH, cur_token, NULL);
            if (!ydb)
            {
                _log_error("fail to register usr_func!\n");
                return -1;
            }
        }
        cur_token = va_arg(args, char *);
        p_ydb = ydb;
    }
    va_end(args);
    if (ydb)
    {
        if (ydb->type != YMLDB_BRANCH)
        {
            _log_error("usr_func can be registered to ymldb branch.!\n");
            return -1;
        }
        if (ydb->callback)
        {
            free(ydb->callback);
        }
        ydb->callback = malloc(sizeof(struct ymldb_callback));
        if (ydb->callback)
        {
            ydb->callback->usr_data = usr_data;
            ydb->callback->usr_func = usr_func;
            ydb->callback->deleted = 0;
            _log_debug("callback %p registered...\n", ydb->callback);
            return 0;
        }
    }
    _log_error("fail to register usr_func..\n");
    return -1;
}

int _ymldb_callback_unregister(char *major_key, ...)
{
    struct ymldb *ydb;
    struct ymldb_cb *cb;
    _log_empty();
    if (!(cb = _ymldb_cb(major_key)))
    {
        _log_error("no ymldb key found.\n");
        return -1;
    }
    _log_debug("\n  @@ g_alloc_count %d @@\n\n", g_alloc_count);
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
            free(ydb->callback);
        }
        ydb->callback = NULL;
        return 0;
    }
    _log_error("unreachable here.\n");
    return -1;
}

static int _ymldb_callback_cmp(void *v1, void *v2)
{
    struct ymldb_callback *c1 = v1;
    struct ymldb_callback *c2 = v2;
    if (c1->usr_func == c2->usr_func)
    {
        return c1->usr_data - c2->usr_data;
    }
    return c1->usr_func - c2->usr_func;
}

void *_ymldb_callback_dup(void *src)
{
    struct ymldb_callback *c_src = src;
    struct ymldb_callback *c_dest = malloc(sizeof(struct ymldb_callback));
    if (c_dest)
    {
        c_dest->usr_data = c_src->usr_data;
        c_dest->usr_func = c_src->usr_func;
        c_dest->deleted = c_src->deleted;
    }
    return c_dest;
}

static void _ymldb_callback_set(struct ymldb_params *params, struct ymldb *ydb, int deleted)
{
    do
    {
        if (ydb->callback)
        {
            // _log_debug("cb %p found\n", ydb->callback);
            if (!params->callbacks)
            {
                params->callbacks = cp_avltree_create_by_option(
                    COLLECTION_MODE_NOSYNC | COLLECTION_MODE_COPY | COLLECTION_MODE_DEEP,
                    _ymldb_callback_cmp, _ymldb_callback_dup, free, _ymldb_callback_dup, free);
            }
            if (params->callbacks)
            {
                _log_debug("cb %p added\n", ydb->callback);
                ydb->callback->deleted = deleted;
                cp_avltree_insert(params->callbacks, ydb->callback, ydb->callback);
            }
            break;
        }
        ydb = ydb->parent;
    } while (ydb);
}

static int _ymldb_callback_entry(void *n, void *dummy)
{
    cp_avlnode *node = n;
    struct ymldb_callback *callback = node->key;
    _log_debug("cb %p run\n", callback);
    return callback->usr_func(callback->usr_data, callback->deleted);
}

static void _ymldb_callback_run(struct ymldb_params *params)
{
    if (!params->callbacks)
        return;
    _log_debug("cb exec phase! (count=%d)\n", cp_avltree_count(params->callbacks));
    cp_avltree_callback(params->callbacks, _ymldb_callback_entry, NULL);
    cp_avltree_destroy(params->callbacks);
    params->callbacks = NULL;
}
