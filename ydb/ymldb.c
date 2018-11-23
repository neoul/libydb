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

#include "yalloc.h"
#include "ylist.h"
#include "ytree.h"

// #undef strdup
// #define strdup ystrdup
// #define malloc yalloc
// #define free yfree

#include "ymldb.h"

FILE *instream_mointor;
FILE *outstream_monitor;

typedef enum ymldb_type_e
{
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
    int no_record : 1;
    int level : 31;
    ymldb_type_t type;
    union {
        ytree *children;
        char *value;
    };
    struct ynode *parent;
    struct ycallback *callback;
};

struct ystream
{
    FILE *stream;
    size_t len;
    size_t maxlen;
    int writable : 1;
    int dynamic : 1;
    int allocated : 1;
    int options : 29;
    char *buf;
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

struct sbuf
{
    char buf[YMLDB_STREAM_BUF_SIZE];
    int len;
    int res;
};

#define sbuf_write(STRBUF, ...)                                                                                \
    do                                                                                                         \
    {                                                                                                          \
        if ((STRBUF)->res)                                                                                     \
            break;                                                                                             \
        int len = snprintf((STRBUF)->buf + (STRBUF)->len, YMLDB_STREAM_BUF_SIZE - (STRBUF)->len, __VA_ARGS__); \
        if (len < 0)                                                                                           \
            (STRBUF)->res = -1;                                                                                \
        else                                                                                                   \
            (STRBUF)->len += len;                                                                              \
    } while (0)

void sbuf_free(struct sbuf *sbuf)
{
    if (sbuf)
        free(sbuf);
}

struct sbuf *sbuf_new()
{
    struct sbuf *sbuf = malloc(sizeof(struct sbuf));
    if (!sbuf)
        return NULL;
    sbuf->buf[0] = 0;
    sbuf->len = 0;
    sbuf->res = 0;
    return sbuf;
}

void sbuf_clean(struct sbuf *sbuf)
{
    if (sbuf)
    {
        sbuf->buf[0] = 0;
        sbuf->len = 0;
        sbuf->res = 0;
    }
}

void sbuf_printf_head(struct sbuf *sbuf, unsigned int opcode, unsigned int sequence)
{
    sbuf_write(sbuf, "# @@\n");
    sbuf_write(sbuf, "# %u\n", sequence);

    // %TAG !merge! actusnetworks.com:op:
    if (opcode & YMLDB_OP_SEQ)
    {
        if (opcode & YMLDB_OP_SEQ_CON)
            sbuf_write(sbuf, "%s %s %s%u\n", "%TAG", YMLDB_TAG_OP_SEQ, YMLDB_TAG_SEQ_CON, sequence);
        else
            sbuf_write(sbuf, "%s %s %s%u\n", "%TAG", YMLDB_TAG_OP_SEQ, YMLDB_TAG_SEQ, sequence);
    }

    if (opcode & YMLDB_OP_ACK)
    {
        sbuf_write(sbuf, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_ACK, YMLDB_TAG_ACK);
    }

    if (opcode & YMLDB_OP_MERGE)
    {
        sbuf_write(sbuf, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_MERGE, YMLDB_TAG_MERGE);
    }
    else if (opcode & YMLDB_OP_DELETE)
    {
        sbuf_write(sbuf, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_DELETE, YMLDB_TAG_DELETE);
    }
    else if (opcode & YMLDB_OP_GET)
    {
        sbuf_write(sbuf, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_GET, YMLDB_TAG_GET);
    }
    else if (opcode & YMLDB_OP_SYNC)
    {
        sbuf_write(sbuf, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_SYNC, YMLDB_TAG_SYNC);
    }
    else if (opcode & YMLDB_OP_NOOP)
    {
        sbuf_write(sbuf, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_NOOP, YMLDB_TAG_NOOP);
    }

    if (opcode & YMLDB_OP_SUBSCRIBER)
    {
        sbuf_write(sbuf, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_SUBSCRIBER, YMLDB_TAG_SUBSCRIBER);
    }
    else if (opcode & YMLDB_OP_PUBLISHER)
    {
        sbuf_write(sbuf, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_PUBLISHER, YMLDB_TAG_PUBLISHER);
    }

    sbuf_write(sbuf, "---\n");
}

void sbuf_printf_tail(struct sbuf *sbuf)
{
    sbuf_write(sbuf, "\n...\n\n");
}

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
    struct sbuf *outbuf;
    struct ystream *dupstream;
    int resv : 27;
    int no_record : 1;
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
    if (!pid)
    {
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

#define _log_entrance()                                                                                 \
    do                                                                                                  \
    {                                                                                                   \
        if (g_ymldb_log < YMLDB_LOG_LOG)                                                                \
            break;                                                                                      \
        FILE *_log_stream = _log_open(stdout);                                                          \
        if (!_log_stream)                                                                               \
            break;                                                                                      \
        _log_write(_log_stream, "\n________________________________\nP%d:%s >>>\n", pid, __FUNCTION__); \
        _log_close(_log_stream);                                                                        \
    } while (0)

#define _log_debug(...)                                                                    \
    do                                                                                     \
    {                                                                                      \
        if (g_ymldb_log < YMLDB_LOG_LOG)                                                   \
            break;                                                                         \
        FILE *_log_stream = _log_open(stdout);                                             \
        if (!_log_stream)                                                                  \
            break;                                                                         \
        _log_write(_log_stream, "[ymldb:debug] P%d:%s:%d: ", pid, __FUNCTION__, __LINE__); \
        _log_write(_log_stream, __VA_ARGS__);                                              \
        _log_close(_log_stream);                                                           \
    } while (0)

#define _log_info(...)                                                                    \
    do                                                                                    \
    {                                                                                     \
        if (g_ymldb_log < YMLDB_LOG_INFO)                                                 \
            break;                                                                        \
        FILE *_log_stream = _log_open(stdout);                                            \
        if (!_log_stream)                                                                 \
            break;                                                                        \
        _log_write(_log_stream, "[ymldb:info] P%d:%s:%d: ", pid, __FUNCTION__, __LINE__); \
        _log_write(_log_stream, __VA_ARGS__);                                             \
        _log_close(_log_stream);                                                          \
    } while (0)

#define _log_error(...)                                                          \
    do                                                                           \
    {                                                                            \
        if (g_ymldb_log < YMLDB_LOG_ERR)                                         \
            break;                                                               \
        FILE *_log_stream = _log_open(stderr);                                   \
        if (!_log_stream)                                                        \
            break;                                                               \
        _log_write(_log_stream, "\n  [ymldb:error]\n\n");                        \
        _log_write(_log_stream, "\tP%d:%s:%d\n\t", pid, __FUNCTION__, __LINE__); \
        _log_write(_log_stream, __VA_ARGS__);                                    \
        _log_write(_log_stream, "\n");                                           \
        _log_close(_log_stream);                                                 \
    } while (0)

#define _log_error_head()                                                      \
    do                                                                         \
    {                                                                          \
        if (g_ymldb_log < YMLDB_LOG_ERR)                                       \
            break;                                                             \
        FILE *_log_stream = _log_open(stderr);                                 \
        if (!_log_stream)                                                      \
            break;                                                             \
        _log_write(_log_stream, "\n  [ymldb:error]\n\n");                      \
        _log_write(_log_stream, "\tP%d:%s:%d\n", pid, __FUNCTION__, __LINE__); \
        _log_close(_log_stream);                                               \
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

int _log_error_parser(yaml_parser_t *parser)
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

static void _params_free(struct ymldb_params *params);
static struct ymldb_params *_params_alloc(struct ymldb_cb *cb, FILE *instream, FILE *outstream);

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

#define S10 "          "
static char *g_space = S10 S10 S10 S10 S10 S10 S10 S10 S10 S10;

static struct ynode *g_ydb = NULL;
static ytree *g_ycb = NULL;
static ytree *g_fds = NULL;
static ylist *g_callbacks = NULL;

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
    if (opcode & YMLDB_OP_NOOP)
        strcat(opstr, "no-op|");
    strcat(opstr, ")");
    return opstr;
}

struct ymldb_cb *_ymldb_cb(char *major_key)
{
    if (major_key && g_ycb)
    {
        struct ymldb_cb *cb = ytree_search(g_ycb, major_key);
        if (cb)
            return cb;
    }
    return NULL;
}

static ylist *_ymldb_traverse_ancestors(struct ynode *ydb, int traverse_level)
{
    if (!ydb)
        return NULL;
    ylist *templist = ylist_create();
    ydb = ydb->parent;
    while (ydb && ydb->level >= traverse_level)
    {
        ylist_push_front(templist, ydb);
        ydb = ydb->parent;
    }
    return templist;
}

static void _ymldb_traverse_free(ylist *templist)
{
    if (templist)
    {
        ylist_destroy(templist);
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
static int _ymldb_fprintf_each_of_node(void *key, void *data, void *dummy)
{
    struct ynode *ydb = data;
    FILE *stream = dummy;
    _ymldb_fprintf_node(stream, ydb, ydb->level, 0); // not print parents
    return 0;
}

static void _ymldb_fprintf_node(FILE *stream, struct ynode *ydb, int print_level, int no_print_children)
{
    if (!ydb)
        return;
    if (print_level < ydb->level)
    { // print parents
        ylist_iter *iter;
        ylist *ancestors;

        ancestors = _ymldb_traverse_ancestors(ydb, print_level);
        for (iter = ylist_first(ancestors); !ylist_done(ancestors, iter); iter = ylist_next(ancestors, iter))
        {
            struct ynode *ancestor;
            ancestor = ylist_data(iter);
            if (ancestor->level == 0) // skip level 0
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
        ytree_traverse(ydb->children, _ymldb_fprintf_each_of_node, stream);
    }
    else if (ydb->type == YMLDB_LEAFLIST)
        fprintf(stream, "%.*s- %s\n", (ydb->level - 1) * 2, g_space, _str_dump(ydb->key));
    else
        fprintf(stream, "%.*s%s: %s\n", (ydb->level - 1) * 2, g_space, ydb->key, _str_dump(ydb->value));
    return;
}

void _ymldb_fprintf_head(FILE *stream, unsigned int opcode, unsigned int sequence)
{
    fprintf(stream, "# @@\n");
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
    else if (opcode & YMLDB_OP_NOOP)
    {
        fprintf(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_NOOP, YMLDB_TAG_NOOP);
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

struct ystream *_ystream_open_to_write(int buflen)
{
    struct ystream *ystream;
    ystream = malloc(sizeof(struct ystream));
    if (ystream)
    {
        memset(ystream, 0x0, sizeof(struct ystream));
        if (buflen > 0)
        {
            ystream->buf = malloc(buflen);
            if (!ystream->buf)
            {
                free(ystream);
                return NULL;
            }
            // memset(ystream->buf, 0x0, buflen);
            ystream->buf[0] = 0;
            ystream->len = 0;
            ystream->maxlen = buflen;
            ystream->stream = fmemopen(ystream->buf, ystream->maxlen, "w");
            setbuf(ystream->stream, NULL);
            ystream->dynamic = 0;
        }
        else
        {
            ystream->stream = open_memstream(&(ystream->buf), &(ystream->len));
            if (!ystream->stream)
            {
                free(ystream);
                return NULL;
            }
            ystream->dynamic = 1;
        }
    }
    ystream->writable = 1;
    ystream->allocated = 1;
    return ystream;
}

struct ystream *_ystream_reopen_to_read(struct ystream *ystream)
{
    if (ystream)
    {
        if (ystream->stream)
        {
            if (!ystream->dynamic)
            {
                if (ystream->len <= 0)
                    ystream->len = ftell(ystream->stream);
                ystream->buf[ystream->len] = 0;
            }
            fclose(ystream->stream);
        }
        ystream->stream = NULL;
        if (!ystream->buf)
            goto open_failed;
        ystream->stream = fmemopen(ystream->buf, ystream->len, "r");
        ystream->writable = 0;
        if (!ystream->stream)
            goto open_failed;
    }
    return ystream;
open_failed:
    if (ystream->stream)
        fclose(ystream->stream);
    if (ystream->buf && ystream->allocated)
        free(ystream->buf);
    free(ystream);
    return NULL;
}

struct ystream *_ystream_open_to_read_from_buf(char *buf, int buflen)
{
    struct ystream *ystream;
    if (!buf || buflen < 0)
        return NULL;
    ystream = malloc(sizeof(struct ystream));
    if (ystream)
    {
        memset(ystream, 0x0, sizeof(struct ystream));
        ystream->buf = buf;
        ystream->len = buflen;
        ystream->stream = fmemopen(ystream->buf, ystream->len, "r");
        if (!ystream->stream)
        {
            free(ystream);
            return NULL;
        }
        // ystream->writable = 0;
        // ystream->dynamic = 0;
        ystream->allocated = 0;
    }
    return ystream;
}

void _ystream_free(struct ystream *ystream)
{
    if (ystream->stream)
        fclose(ystream->stream);
    if (ystream->buf && ystream->allocated)
        free(ystream->buf);
    free(ystream);
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
            ytree *children = ydb->children;
            ydb->children = NULL;
            ytree_destroy_custom(children, _ymldb_node_free);
        }

        _notify_callback_run(ydb, 1);
        if (ydb->callback)
        {
            int deleted = ydb->callback->deleted ? 0 : 1;
            _callback_unreg(ydb, deleted);
        }

        if (ydb->type != YMLDB_BRANCH)
        {
            if (ydb->value)
                yfree(ydb->value);
        }
        yfree(ydb->key);
        free(ydb);
    }
}

// Remove all subtree and data
static void _ymldb_node_free_without_callback(void *vdata)
{
    struct ynode *ydb = vdata;
    if (ydb)
    {
        _log_debug("@@ no-record (%s)\n", ydb->key);
        if (ydb->type == YMLDB_BRANCH)
        {
            // fixed BUG - prevent to loop infinitely in children free phase.
            ytree *children = ydb->children;
            ydb->children = NULL;
            ytree_destroy_custom(children, _ymldb_node_free_without_callback);
        }
        _callback_free(ydb->callback);

        if (ydb->type != YMLDB_BRANCH)
        {
            if (ydb->value)
                yfree(ydb->value);
        }
        yfree(ydb->key);
        free(ydb);
    }
}

// Remove all no-record ynodes
static int _ymldb_node_free_no_record(void *key, void *data, void *dummy)
{
    struct ynode *ydb = data;
    if (ydb)
    {
        if (ydb->no_record)
        {
            ylist *del_list = dummy;
            ylist_push_back(del_list, ydb);
        }
        else
        {
            if (ydb->type == YMLDB_BRANCH)
            {
                ytree_traverse(ydb->children, _ymldb_node_free_no_record, dummy);
            }
        }
    }
    return 0;
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
    struct ynode *ydb = NULL;
    if (parent)
    {
        if (parent->type != YMLDB_BRANCH)
        {
            _log_error_head();
            _log_error_body("Unable to assign new ymldb to a leaf ymldb node.\n");
            _log_error_body(" - parent: %s, child %s\n", parent->key, key);
            return NULL;
        }

        ydb = ytree_search(parent->children, key);
        if (ydb)
        {
            // check if the key exists.
            // notify the change if they differ.
            if (ydb->type != type)
            {
                _log_debug("different type (%s %s-->%s)\n",
                           ydb->key, _ydb_type(ydb->type), _ydb_type(type));
                if (ydb->type == YMLDB_BRANCH)
                {
                    if(ydb->children)
                        ytree_destroy_custom(ydb->children, _ymldb_node_free);
                }
                else
                {
                    if (ydb->value)
                        yfree(ydb->value);
                }

                ydb->type = type;
                if (ydb->type == YMLDB_BRANCH)
                {
                    ydb->children = ytree_create((ytree_cmp)strcmp, NULL);
                    if (!ydb->children)
                        goto free_ydb;
                }
                else if (type == YMLDB_LEAFLIST)
                {
                    ydb->value = ystrdup(key);
                    if (!ydb->value)
                        goto free_ydb;
                }
                else
                {
                    ydb->value = ystrdup(value);
                    if (!ydb->value)
                        goto free_ydb;
                }
                _ymldb_node_merge_reply(params, ydb);
            }
            else if (ydb->type == YMLDB_LEAF)
            {
                if (strcmp(ydb->value, value) != 0)
                {
                    yfree(ydb->value);
                    ydb->value = ystrdup(value);
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
    ydb->type = type;
    ydb->key = ystrdup(key);
    if (!ydb->key)
        goto free_ydb;

    if (type == YMLDB_BRANCH)
    {
        ydb->children = ytree_create((ytree_cmp)strcmp, NULL);
        if (!ydb->children)
            goto free_ydb;
    }
    else if (type == YMLDB_LEAFLIST)
    {
        ydb->value = ystrdup(key);
        if (!ydb->value)
            goto free_ydb;
    }
    else
    {
        ydb->value = ystrdup(value);
        if (!ydb->value)
            goto free_ydb;
    }

    if (parent)
    {
        ydb->parent = parent;
        ydb->level = parent->level + 1;
        ytree_insert(parent->children, ydb->key, ydb);
    }
    else
    {
        ydb->parent = NULL;
        ydb->level = 0;
    }
    // _log_debug("ydb->key %s ydb->type %d ydb->value '%s'\n", ydb->key, ydb->type, ydb->value);
    // notify_ydb:
    if (params && params->no_record)
        ydb->no_record = 1;
    _ymldb_node_merge_reply(params, ydb);
    return ydb;

free_ydb:
    _log_error("mem alloc failed for ymldb node.\n");
    if (ydb)
    {
        if (type == YMLDB_BRANCH)
        {
            if(ydb->children)
                ytree_destroy_custom(ydb->children, _ymldb_node_free);
        }
        else
        {
            if (ydb->value)
                yfree(ydb->value);
        }
    }

    if (ydb->key)
        yfree(ydb->key);
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

    ydb = ytree_search(parent->children, key);
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
        _log_debug("'%s' doesn't exists\n", key);
        return;
    }

    params->no_change = 0;
    print_level = _ymldb_print_level(params->last_ydb, ydb);
    _params_buf_dump(params, ydb, print_level, 1);
    // parent should be saved because of the ydb will be removed.
    params->last_ydb = parent;
    ydb = ytree_delete(parent->children, key);
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
        ydb = ytree_search(parent->children, key);
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
                    ydb = ytree_search(p_ydb->children, key);
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
                    ydb = ytree_search(p_ydb->children, key);
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
                ydb = ytree_search(p_ydb->children, key);
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
                    ydb = ytree_search(p_ydb->children, key);
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
                ydb = ytree_search(p_ydb->children, key);
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
    struct sbuf *outbuf = params->outbuf;
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
                sbuf_write(outbuf, "%.*s- %s\n", level * 2, g_space, key);
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
                sbuf_write(outbuf, "%.*s%s: %s\n", level * 2, g_space, key, value);
            }
            else
            { // not leaf
                _log_debug("## %s\n", key);
                sbuf_write(outbuf, "%.*s%s:\n", level * 2, g_space, key);
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
        sbuf_write(outbuf, "%.*s%s\n", level * 2, g_space, key);
    }
    break;
    case YAML_NO_NODE:
    default:
        break;
    }
    return 0;
}

int _ymldb_internal_updated_fd_flags(struct ymldb_params *params)
{
    struct ymldb_cb *cb = params->cb;
    int is_pub = params->in.opcode & YMLDB_OP_PUBLISHER;
    int i;
    for (i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
    {
        if (cb->fd_subscriber[i] == cb->fd_requester)
        {
            cb->fd_flags[i] = cb->fd_flags[i] | (is_pub)?YMLDB_FLAG_PUBLISHER:YMLDB_FLAG_SUBSCRIBER;
            _log_debug("fd (%d) is %s\n", cb->fd_requester, (is_pub)?"publisher":"subscriber");
            break;
        }
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
            else if (strcmp(op, YMLDB_TAG_OP_NOOP) == 0)
            {
                opcode = opcode | YMLDB_OP_NOOP;
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
    if(!opcode) // default opcode (merge)
        opcode = opcode | YMLDB_OP_MERGE;
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
    iop_update_fd_flags,
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

    // no-operation
    if(in_opcode & YMLDB_OP_NOOP)
    {
        if(ack)
            goto _done;
        if (flags & YMLDB_FLAG_PUBLISHER)
        {
            out_opcode |= YMLDB_OP_PUBLISHER;
            if (in_opcode & YMLDB_OP_SUBSCRIBER)
                iop = iop_update_fd_flags;
            else if (in_opcode & YMLDB_OP_PUBLISHER)
                iop = iop_update_fd_flags;
            else
            {
                iop = iop_relay;
                request_and_reply = 1;
            }
        }
        else if (flags & YMLDB_FLAG_SUBSCRIBER)
        {
            out_opcode |= YMLDB_OP_SUBSCRIBER;
            if (in_opcode & YMLDB_OP_SUBSCRIBER)
                iop = iop_ignore;
            else if (in_opcode & YMLDB_OP_PUBLISHER)
                iop = iop_ignore;
            else
            {
                iop = iop_relay;
                request_and_reply = 1;
            }
        }
        else
        {
            iop = iop_ignore;
        }
        out_opcode |= YMLDB_OP_NOOP;
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
                    (in_opcode & YMLDB_OP_MERGE) ? iop_merge : (in_opcode & YMLDB_OP_DELETE) ? iop_delete : (in_opcode & YMLDB_OP_GET) ? iop_get : (in_opcode & YMLDB_OP_SYNC) ? iop_relay : iop_ignore;

                out_opcode |=
                    (in_opcode & YMLDB_OP_MERGE) ? YMLDB_OP_MERGE : (in_opcode & YMLDB_OP_DELETE) ? YMLDB_OP_DELETE : (in_opcode & YMLDB_OP_SYNC) ? YMLDB_OP_SYNC : 0;
                if (in_opcode & YMLDB_OP_SYNC)
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
               (iop == iop_ignore) ? "ignore" : (iop == iop_merge) ? "merge" : (iop == iop_delete) ? "delete" : (iop == iop_relay) ? "relay" : (iop == iop_get) ? "get" : (iop == iop_relay_delete) ? "relay_and_delete" : (iop == iop_update_fd_flags) ? "iop_update_fd_flags" :  "-" );

    return iop;
}

static void _params_free(struct ymldb_params *params)
{
    if (!params)
        return;
    if(params->dupstream)
        _ystream_free(params->dupstream);
    if (params->outbuf)
        sbuf_free(params->outbuf);
    yaml_parser_delete(&params->parser);
    free(params);
    return;
}

static struct ymldb_params *_params_alloc(struct ymldb_cb *cb, FILE *instream, FILE *outstream)
{
    struct ymldb_params *params;
    params = malloc(sizeof(struct ymldb_params));
    if (!params)
    {
        _log_error("ymldb_params alloc failed.\n");
        return NULL;
    }
    memset(params, 0, sizeof(struct ymldb_params));

    if (!yaml_parser_initialize(&params->parser))
    {
        _log_error_parser(&params->parser);
        yaml_parser_delete(&params->parser);
        free(params);
        return NULL;
    }
    if (instream_mointor)
    {
        char c;
        struct ystream *dupstream;
        dupstream = _ystream_open_to_write(0);
        if (!dupstream)
        {
            _log_error("fail to open ymldb stream\n");
            yaml_parser_delete(&params->parser);
            free(params);
            return NULL;
        }
        // Read contents from file
        c = fgetc(instream);
        while (!feof(instream) && c != 0)
        {
            fputc(c, dupstream->stream);
            fputc(c, instream_mointor);
            c = fgetc(instream);
        }
        params->dupstream = dupstream;
        _ystream_reopen_to_read(dupstream);
        yaml_parser_set_input_file(&params->parser, dupstream->stream);
    }
    else
    {
        yaml_parser_set_input_file(&params->parser, instream);
    }
    params->in.stream = instream;
    params->out.stream = outstream;
    params->in.opcode = 0;
    params->out.opcode = 0;
    params->in.sequence = 0;
    params->out.sequence = 0;
    params->outbuf = sbuf_new();
    if (!params->outbuf)
    {
        _log_error("ymldb_params->outbuf alloc failed.\n");
        yaml_parser_delete(&params->parser);
        free(params);
        return NULL;
    }
    params->res = 0;
    params->cb = cb;
    params->no_record = (cb->flags & YMLDB_FLAG_NO_RECORD) ? 1 : 0;
    return params;
}

static int _params_yaml_load(struct ymldb_params *params)
{
    if (!yaml_parser_load(&params->parser, &params->document))
    {
        _log_error_parser(&params->parser);
        return -1;
    }
    return 0;
}

static int _params_yaml_empty(struct ymldb_params *params)
{
    if(yaml_document_get_root_node(&params->document))
        return 0;
    return 1; // empty
}

static void _params_yaml_unload(struct ymldb_params *params)
{
    yaml_document_delete(&params->document);
}

static int _params_buf_init(struct ymldb_params *params)
{
    params->last_ydb = NULL;
    sbuf_clean(params->outbuf);
    sbuf_printf_head(params->outbuf, params->out.opcode, params->out.sequence);
    return 0;
}

static int _params_buf_dump_children(void *key, void *data, void *dummy)
{
    struct ynode *ydb = data;
    struct ymldb_params *params = dummy;
    _params_buf_dump(params, ydb, ydb->level, 0); // not print parents
    return 0;
}

static void _params_buf_dump(struct ymldb_params *params, struct ynode *ydb, int print_level, int no_print_children)
{
    int flushed = 0;
    struct sbuf *outbuf = NULL;
    if (!ydb)
        return;
    flushed = _params_buf_flush(params, 0);
    if (flushed)
    {
        _log_debug("sent buf is flushed\n");
        _params_buf_init(params);
        _log_debug("print_level %d\n", print_level);
        _log_debug("cur ydb->key %s ydb->level %d\n", ydb->key, ydb->level);
        print_level = 0;
    }
    outbuf = params->outbuf;
    // _log_debug("outbuf %p, print_level %d\n", outbuf, print_level);
    if (print_level < ydb->level)
    { // print parents
        ylist_iter *iter;
        ylist *ancestors;

        ancestors = _ymldb_traverse_ancestors(ydb, print_level);
        for (iter = ylist_first(ancestors); !ylist_done(ancestors, iter); iter = ylist_next(ancestors, iter))
        {
            struct ynode *ancestor;
            ancestor = ylist_data(iter);
            if (ancestor->level <= 1)
                continue;
            switch (ancestor->type)
            {
            case YMLDB_BRANCH:
                sbuf_write(outbuf, "%.*s%s:\n", (ancestor->level - 2) * 2, g_space, ancestor->key);
                break;
            case YMLDB_LEAFLIST:
                sbuf_write(outbuf, "%.*s- %s\n", (ancestor->level - 2) * 2, g_space, ancestor->key);
                break;
            case YMLDB_LEAF:
                sbuf_write(outbuf, "%.*s%s: %s\n", (ancestor->level - 2) * 2, g_space, ancestor->key, ancestor->value);
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
            sbuf_write(outbuf, "%.*s%s:\n", (ydb->level - 2) * 2, g_space, ydb->key);
        }
        if (no_print_children)
            goto end;
        ytree_traverse(ydb->children, _params_buf_dump_children, params);
    }
    else if (ydb->type == YMLDB_LEAFLIST)
    {
        sbuf_write(outbuf, "%.*s- %s\n", (ydb->level - 2) * 2, g_space, _str_dump(ydb->key));
    }
    else
    {
        sbuf_write(outbuf, "%.*s%s: %s\n", (ydb->level - 2) * 2, g_space, ydb->key, _str_dump(ydb->value));
    }
end:
    params->last_ydb = ydb;
    return;
}

// Return 1 if reply.stream is flushed, otherwise 0.
static int _params_buf_flush(struct ymldb_params *params, int forced)
{
    struct sbuf *outbuf;
    if (!params || !params->outbuf)
        return 0;
    outbuf = params->outbuf;
    if (forced)
    {
        goto flushing;
    }
    else if (outbuf->len >= YMLDB_STREAM_THRESHOLD || outbuf->res)
    {
        goto flushing;
    }
    return 0;
flushing:
    sbuf_printf_tail(outbuf);
    if (!forced)
    {
        if (params->out.opcode & YMLDB_OP_SEQ)
        {
            char *seq_tag = strstr(outbuf->buf, YMLDB_TAG_SEQ_BASE);
            if (seq_tag)
                strncpy(seq_tag, YMLDB_TAG_SEQ_CON, strlen(YMLDB_TAG_SEQ_CON));
        }
    }

    _log_debug("@@ %d %s\n\n", outbuf->len, outbuf->buf);

    _log_debug("@@ inprogress_cnt %d\n", params->cb->inprogress_cnt);
    if (params->cb->inprogress_cnt > 1)
    {
        // sub operation output is disabled..
        _log_debug("@@ ignored output.\n");
        return 1;
    }

    if (params->out.stream)
    {
        fputs(outbuf->buf, params->out.stream);
        fflush(params->out.stream);
    }

    if (outstream_monitor)
    {
        fputs(outbuf->buf, outstream_monitor);
        fflush(outstream_monitor);
    }

    if (params->cb->flags & YMLDB_FLAG_CONN)
        _distribution_send(params);
    return 1;
}

static int _params_buf_reset(struct ymldb_params *params)
{
    struct sbuf *outbuf = params->outbuf;
    sbuf_clean(outbuf);
    sbuf_printf_head(outbuf, params->out.opcode, params->out.sequence);
    return 0;
}

static int _ymldb_run(struct ymldb_cb *cb, FILE *instream, FILE *outstream)
{
    int res = 0;
    int done = 0;
    int updated = 0;
    int flush_count = 0;
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
        if (_params_yaml_load(params))
        {
            params->res--;
            break;
        }

        _params_opcode_extract(params);
        done = _params_yaml_empty(params);
        if (!done)
        {
            enum internal_op iop;
            iop = _ymldb_sm(params);
            // sync wait - done
            if (cb->flags & YMLDB_FLAG_INSYNC)
                if (!(params->in.opcode & YMLDB_OP_SEQ_CON))
                    cb->flags = cb->flags & (~YMLDB_FLAG_INSYNC);

            _params_buf_init(params);
            if (iop == iop_ignore)
            {
                _log_debug("in %uth %s\n", params->in.sequence, "ignored ...");
                params->res--;
            }
            else
            {
                if (iop == iop_merge)
                {
                    _ymldb_internal_merge(params, cb->ydb, 1, 1);
                    updated = 1;
                }
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
                else if (iop_update_fd_flags)
                {
                    _ymldb_internal_updated_fd_flags(params);
                }
                _params_buf_flush(params, 1); // forced flush!
                flush_count++;
            }
            _log_debug("result: %s\n", params->res < 0 ? "failed" : "ok");
        }
        _params_yaml_unload(params);
    }
    // if (flush_count > 0)
    //     _params_buf_flush(params, 1); // forced flush!

    _log_debug("<<<\n");
    res = params->res;
    _params_free(params);
    _notify_callback_run_pending();
    if (cb->flags & YMLDB_FLAG_NO_RECORD && updated)
    {
        // remove no_record ynodes.
        if (cb->ydb && cb->ydb->type == YMLDB_BRANCH)
        {
            struct ynode *del_ynode;
            ylist *del_list = ylist_create();
            ytree_traverse(cb->ydb->children, _ymldb_node_free_no_record, del_list);
            while ((del_ynode = ylist_pop_front(del_list)) != NULL)
            {
                struct ynode *parent = del_ynode->parent;
                ytree_delete(parent->children, del_ynode->key);
                _ymldb_node_free_without_callback((void *)del_ynode);
            }
            _log_debug("no-record operation is done!\n");
            ylist_destroy(del_list);
        }
    }
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
        g_ycb = ytree_create((ytree_cmp)strcmp, NULL);
        if (!g_ycb)
        {
            _log_error("g_ycb failed.\n");
            return -1;
        }
    }

    if (ytree_search(g_ydb->children, major_key))
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

    ytree_insert(g_ycb, cb->key, cb);

    if (flags & YMLDB_FLAG_ASYNC)
        cb->flags |= YMLDB_FLAG_ASYNC;
    if (flags & YMLDB_FLAG_NO_RECORD)
        cb->flags |= YMLDB_FLAG_NO_RECORD;
    if (flags & YMLDB_FLAG_NO_RELAY_TO_SUB_PUBLISHER)
        cb->flags |= YMLDB_FLAG_NO_RELAY_TO_SUB_PUBLISHER;
        
    if (flags & (YMLDB_FLAG_PUBLISHER | YMLDB_FLAG_SUBSCRIBER | YMLDB_FLAG_SUB_PUBLISHER))
        _distribution_init(cb, flags);

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
		_log_debug("cb->ydb->key %s cb->ydb->parent %p\n", ydb->key, ydb->parent);

        if (ydb->parent && ydb->parent->children && ydb->type == YMLDB_BRANCH && ydb->key)
		{
            ytree_delete(ydb->parent->children, ydb->key);
		}
		_log_debug("ydb->parent->count %d\n", ytree_size(ydb->parent->children));
        _ymldb_node_free(ydb);
		cb->ydb = NULL;
        _log_debug("\n");
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

    ytree_delete(g_ycb, cb->key);
    _ymldb_destroy(cb);
    if (ytree_size(g_ycb) <= 0)
    {
        _ymldb_node_free(g_ydb);
        ytree_destroy(g_ycb);
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
        ytree_destroy_custom(g_ycb, _ymldb_destroy);
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
        if (g_fds)
        {
            ytree_delete(g_fds, &cb->fd_publisher);
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
                if (g_fds)
                {
                    ytree_delete(g_fds, &cb->fd_subscriber[i]);
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
        _log_debug("g_fds count %d\n", ytree_size(g_fds));
        if (ytree_size(g_fds) <= 0)
        {
            ytree_destroy(g_fds);
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
        _distribution_deinit(cb);

    if (!g_fds)
    {
        g_fds = ytree_create(_g_fds_cmp, NULL);
        if (!g_fds)
        {
            _log_error("g_fds failed.\n");
            return -1;
        }
    }

    if (flags & YMLDB_FLAG_ASYNC)
        cb->flags |= YMLDB_FLAG_ASYNC;
    if (flags & YMLDB_FLAG_SUBSCRIBER)
        cb->flags |= YMLDB_FLAG_SUBSCRIBER;
    else if (flags & YMLDB_FLAG_PUBLISHER)
        cb->flags |= YMLDB_FLAG_PUBLISHER;
    else if (flags & YMLDB_FLAG_SUB_PUBLISHER)
        cb->flags |= YMLDB_FLAG_SUB_PUBLISHER;
    else
    {
        _log_error("%s no flags\n", cb->key);
        return -1;
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

    if (flags & YMLDB_FLAG_PUBLISHER)
    { // PUBLISHER
        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            goto sub_pub;
        }
        if (listen(fd, YMLDB_SUBSCRIBER_MAX) < 0)
        {
            _log_error("%s listen failed (%s).\n", cb->key, strerror(errno));
            goto _error;
        }
    }
    else if (flags & YMLDB_FLAG_SUB_PUBLISHER)
    { // sub-publisher
sub_pub:
        cb->flags |= YMLDB_FLAG_PUBLISHER;
        cb->flags |= YMLDB_FLAG_SUB_PUBLISHER;
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        {
            _log_error("%s connect failed (%s).\n", cb->key, strerror(errno));
            goto _error;
        }
        _log_debug("configured as a sub-publisher\n");
        // send what I am
        ymldb_noop(cb->key);
    }
    else if (flags & YMLDB_FLAG_SUBSCRIBER)
    { // SUBSCRIBER
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        {
            _log_error("%s connect failed (%s).\n", cb->key, strerror(errno));
            goto _error;
        }
        // send what I am
        ymldb_noop(cb->key);
        if (!(cb->flags & YMLDB_FLAG_ASYNC))
            ymldb_sync(cb->key);
    }
    
    if (g_fds)
        ytree_insert(g_fds, &cb->fd_publisher, cb);
    _log_debug("%s distribution - done (sock %d)\n", cb->key, fd);
    return fd;
_error:
    cb->fd_publisher = -1;
    cb->flags |= YMLDB_FLAG_RECONNECT;
    if(fd)
     close(fd);
    return -1;
}

static int _distribution_set(void *key, void *data, void *dummy)
{
    struct ymldb_cb *cb = data;
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

static int _distribution_recv(struct ymldb_cb *cb, FILE *outstream, int fd)
{
	char *cur; 
	int res = 0;
    int len = 0;
    int retry = 10;
    int buflen = 0;
    char buf[YMLDB_STREAM_BUF_SIZE+1];
    buf[YMLDB_STREAM_BUF_SIZE] = 0;
    cb->fd_requester = fd;
    if (cb->flags & YMLDB_FLAG_PUBLISHER && !(cb->flags & YMLDB_FLAG_SUB_PUBLISHER))
    {
        if (cb->fd_publisher == fd)
        {
            int i;
            int subfd = accept(cb->fd_publisher, NULL, NULL);
            if (subfd < 0)
            {
                _log_error("accept failed (%s)\n", strerror(errno));
                cb->flags |= YMLDB_FLAG_RECONNECT;
                res = -1;
                goto _done;
            }
            for (i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
            {
                if (cb->fd_subscriber[i] < 0)
                {
                    cb->fd_subscriber[i] = subfd;
                    cb->fd_flags[i] = 0;
                    if (g_fds)
                        ytree_insert(g_fds, &cb->fd_subscriber[i], cb);
                    _log_debug("subscriber (subfd %d) added..\n", subfd);
                    break;
                }
            }
            if (i >= YMLDB_SUBSCRIBER_MAX)
            {
                // return no error, because it is partial error.
                _log_error("subscriber (subfd %d) - adding failed due to over subscription..\n", subfd);
                close(subfd);
                subfd = 0;
            }
            res = subfd;
            goto _done;
        }
    }
read_message:
    if(retry <= 0)
    {
        _log_error("fd %d recv failed - retry %d\n", fd, retry);
        res = 0;
        goto _done;
    }
    // len = read(fd, buf + buflen, YMLDB_STREAM_BUF_SIZE - buflen);
    len = recv(fd, buf + buflen, YMLDB_STREAM_BUF_SIZE - buflen, MSG_DONTWAIT);
    if (len <= 0)
    {
        if(errno == EAGAIN || errno == EWOULDBLOCK)
        {
            retry--;
            usleep(1);
			_log_debug("retry .. remained %d\n", retry);
            goto read_message;
        }
		else if (len < 0)
        {
            _log_error("fd %d read failed (%s)\n", fd, strerror(errno));
        }
        else // (len == 0)
            _log_error("fd %d closed (EOF)\n", fd);

        if (fd == cb->fd_publisher)
        {
            cb->flags |= YMLDB_FLAG_RECONNECT;
        }
        else
        {
            int i;
            if (g_fds)
                ytree_delete(g_fds, &fd);
            for (i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
            {
                if (cb->fd_subscriber[i] == fd)
                {
                    cb->fd_subscriber[i] = -1;
                    cb->fd_flags[i] = 0;
                    close(fd);
                }
            }
        }
        res = -1;
        goto _done;
    }
    buflen += len;
	buf[buflen] = 0;
	
	_log_debug("recv() buflen %d\n", buflen);
    len = 0;
	cur = &buf[len];
    while(buflen > 0)
    {
		char *start = strstr(cur, "# @@\n");
		if(start) {
			// roll forward
            _log_debug("skip string in front of the start delimiter (len %ld)\n", (start - cur));
			len = (start - cur);
			cur = cur + len;
			buflen = buflen - len;
		}
		
		char *end = strstr(cur, "...\n");
        if (start && end)
        {
            FILE *instream;
            len = (end - cur) + 4;
            cur[len] = 0;
            instream = fmemopen(cur, len, "r");
            _log_debug("@@ len=%d buf=\n----------\n%s\n---------\n", (len+1), cur);
            if(instream)
            {
                _ymldb_run(cb, instream, outstream);
                fclose(instream);
            }
            len++;
			cur = cur + len;
            buflen = buflen - len;
            _log_debug("remained len=%d\n", buflen);
        }
        else if(!start)
        {
            len = 0;
            cur = &buf[len];
            buflen = 0;
            _log_debug("discard buf due to no start delimiter (buflen %d)\n", buflen);
        }
        else
        {
            if (buflen >= YMLDB_STREAM_BUF_SIZE)
            {
                _log_error("discard jumbo message larger than rx buffer (buflen %d).\n", buflen);
                res = 0;
                goto _done;
            }
            memcpy(buf, cur, buflen);
			buf[buflen] = 0;
            _log_debug("receive more...(current buflen=%d)\n", buflen);
            _log_debug("remained buf...\n--------------------\n%s\n----------------------\n", buf);
            usleep(1); // for context switching
            goto read_message;
        }
    }
    _log_debug("recv is done.\n");
    res = 0;
_done:
    cb->fd_requester = 0;
    return res;
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
        if (cb->fd_publisher > 0)
            FD_CLR(cb->fd_publisher, set);
        res = _distribution_init(cb, cb->flags);
        return (res >= 0) ? 0 : -1;
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

static int _distribution_recv_each_of_cb(void *key, void *data, void *dummy)
{
    struct ymldb_cb *cb = data;
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
            if (diff < 1000000.0)
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
    struct sbuf *outbuf = params->outbuf;
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
        if (fd <= 0)
        {
            _log_debug("discarded due to unknown request/reply target (fd %d)\n",
                       cb->fd_requester);
            return 0;
        }
    _relay_rewrite:
        res = write(fd, outbuf->buf + sent, outbuf->len - sent);
        if (res < 0)
        {
            cb->flags |= YMLDB_FLAG_RECONNECT;
            _log_error("fd %d send failed (%s)\n", fd, strerror(errno));
            return -1;
        }
        sent = res + sent;
        if (sent < outbuf->len && retry < 3)
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
            if (cb->fd_requester != cb->fd_publisher)
            {
            subscriber_rewrite:
                res = write(cb->fd_publisher, outbuf->buf + sent, outbuf->len - sent);
                if (res < 0)
                {
                    cb->flags |= YMLDB_FLAG_RECONNECT;
                    _log_error("fd %d send failed (%s)\n",
                               cb->fd_publisher, strerror(errno));
                    return -1;
                }
                sent = res + sent;
                if (sent < outbuf->len && retry < 3)
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
                if ((cb->flags & YMLDB_FLAG_NO_RELAY_TO_SUB_PUBLISHER) && 
                    (cb->fd_flags[i] & YMLDB_FLAG_PUBLISHER))
                {
                    _log_debug("no_relay_to_sub_publisher configured (fd %d)\n", cb->fd_subscriber[i]);
                    continue;
                }
                // doesn't relay message to cb->fd_requester
                if (cb->fd_subscriber[i] >= 0 && cb->fd_requester != cb->fd_subscriber[i])
                {
                    sent = 0;
                    retry = 0;
                publisher_rewrite:
                    res = write(cb->fd_subscriber[i], outbuf->buf + sent, outbuf->len - sent);
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
                    if (sent < outbuf->len && retry < 3)
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
    input = _ystream_open_to_write(0);
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

    _ystream_reopen_to_read(input);
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
    input = _ystream_open_to_write(0);
    if (!input)
    {
        _log_error("fail to open ymldb stream\n");
        goto failed;
    }
    output = _ystream_open_to_write(0);
    if (!output)
    {
        _log_error("fail to open ymldb stream\n");
        goto failed;
    }

    _ymldb_fprintf_head(input->stream, opcode, 0);
    _ymldb_remove_specifiers(input->stream, format);
    _ymldb_fprintf_tail(input->stream);

    _ystream_reopen_to_read(input);
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
    input = _ystream_open_to_write(0);
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

    _ystream_reopen_to_read(input);
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
    input = _ystream_open_to_write(0);
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

    _ystream_reopen_to_read(input);
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
                    ydb = ytree_search(ydb->children, cur_token);
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
                    ydb = ytree_search(ydb->children, cur_token);
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
                ydb = ytree_search(ydb->children, keys[i]);
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

void ymldb_dump(FILE *outstream, char *major_key)
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
        _ymldb_fprintf_node(outstream, cb->ydb, 0, 0);
    }
    else
    { // print all..
        _ymldb_fprintf_node(outstream, g_ydb, 0, 0);
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
        ytree_traverse(g_ycb, _distribution_set, &yd);
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
        ytree_traverse(g_ycb, _distribution_recv_each_of_cb, &yd);
    _log_debug("res=%d\n", yd.res);
    return yd.res;
}

int ymldb_distribution_recv(fd_set *set)
{
    return ymldb_distribution_recv_and_dump(NULL, set);
}

// 0 if success, -1 if a critical error occurs, r>0 if a new connection is established.
int ymldb_distribution_recv_fd_and_dump(FILE *outstream, int *cur_fd)
{
    int res;
    struct ymldb_cb *cb;
    _log_entrance();
    if (!cur_fd)
    {
        _log_error("no cur_fd\n");
        return -1;
    }

    if (g_fds)
    {
        cb = ytree_search(g_fds, cur_fd);
        if (cb)
        {
            if (!(cb->flags & YMLDB_FLAG_CONN))
            {
                _log_error("not a subscriber or publisher\n");
                return -1;
            }
            res = _distribution_recv(cb, outstream, *cur_fd);
            if (res < 0)
            {
                if (cb->flags & YMLDB_FLAG_RECONNECT)
                {
                    int new_fd = 0;
                    new_fd = _distribution_init(cb, cb->flags);
                    if (new_fd <= 0)
                    {
                        _log_error("%s distribution re-init failed.\n", cb->key);
                        return res;
                    }
                    else
                    {
                        _log_debug("%s distribution reinit succeed..n", cb->key);
                        *cur_fd = new_fd;
                        return 0; // no error if the initialization succeed.
                    }
                }
            }
            _log_debug("\n");
            return res;
        }
    }
    _log_error("unknown fd (%d) \n", *cur_fd);
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

const char *ymldb_distribution_get_major_key(int fd)
{
    struct ymldb_cb *cb;
    cb = ytree_search(g_fds, &fd);
    if (cb)
        return cb->key;
    return NULL;
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
    if (callback)
    {
        _log_debug("callback %p free\n", callback);
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
        ydb = ytree_search(p_ydb->children, key[i]);
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
        ydb = ytree_search(ydb->children, cur_token);
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
        ydb = ytree_search(p_ydb->children, keys[i]);
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
        ydb = ytree_search(ydb->children, keys[i]);
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
            g_callbacks = ylist_create();
        new_callback = _callback_alloc(callback->type, callback->usr_func, callback->usr_data, callback->ydb);
        if (new_callback && g_callbacks)
        {
            new_callback->meta_data = cd;
            ylist_push_back(g_callbacks, new_callback);
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
    _log_debug("notify callback: %s\n", ylist_empty(g_callbacks) ? "off" : "on");
    while ((callback = ylist_pop_front(g_callbacks)) != NULL)
    {
        callback->usr_func(callback->usr_data, callback->meta_data);
        _callback_free(callback);
    }
    ylist_destroy(g_callbacks);
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
                ydb = ytree_search(ydb->children, cur_token);
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
        if (iter_allocated)
            free(iter);
        return NULL;
    }

    iter->base = ytree_find(ydb->parent->children, ydb->key);
    iter->cur = iter->base;
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
    for (level = 1; level < keys_num; level++)
    {
        if (ydb)
        {
            if (ydb->type == YMLDB_BRANCH && ydb->children)
            {
                ydb = ytree_search(ydb->children, keys[level]);
            }
            else
            {
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
        if (iter_allocated)
            free(iter);
        return NULL;
    }

    iter->base = ytree_find(ydb->parent->children, ydb->key);
    iter->cur = iter->base;
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
    iter->cur = iter->base;
    return 0;
}

int ymldb_iterator_rebase(struct ymldb_iterator *iter)
{
    if (!iter)
        return -1;
    iter->base = iter->cur;
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
        dest->base = src->base;
        dest->cur = src->cur;
    }
    return dest;
}

const char *ymldb_iterator_lookup_down(struct ymldb_iterator *iter, char *key)
{
    struct ynode *ydb = NULL;
    ytree_iter *child = NULL;
    if (!iter)
        return NULL;
    if (!key)
        key = "";
    ydb = (struct ynode *)ytree_data(iter->cur);
    if (!ydb)
        return NULL;
    if (ydb->type == YMLDB_BRANCH)
    {
        child = ytree_find(ydb->children, (void *)key);
        if (child)
        {
            iter->cur = child;
            ydb = (struct ynode *)ytree_data(iter->cur);
            return ydb->key;
        }
        return NULL;
    }
    return NULL;
}

const char *ymldb_iterator_lookup(struct ymldb_iterator *iter, char *key)
{
    struct ynode *ydb = NULL;
    ytree_iter *next = NULL;
    if (!iter)
        return NULL;
    if (!key)
        key = "";
    ydb = (struct ynode *)ytree_data(iter->cur);
    if (!ydb)
        return NULL;
    if (ydb->parent)
    {
        next = ytree_find(ydb->parent->children, (void *)key);
        if (next)
        {
            iter->cur = next;
            ydb = (struct ynode *)ytree_data(iter->cur);
            return ydb->key;
        }
        return NULL;
    }
    return NULL;
}

const char *ymldb_iterator_down(struct ymldb_iterator *iter)
{
    struct ynode *ydb = NULL;
    ytree_iter *child = NULL;
    if (!iter)
        return NULL;
    ydb = (struct ynode *)ytree_data(iter->cur);
    if (!ydb)
        return NULL;
    if (ydb->type == YMLDB_BRANCH)
    {
        child = ytree_first(ydb->children);
        if (child)
        {
            iter->cur = child;
            ydb = (struct ynode *)ytree_data(iter->cur);
            return ydb->key;
        }
        return NULL;
    }
    return NULL;
}

const char *ymldb_iterator_up(struct ymldb_iterator *iter)
{
    struct ynode *ydb = NULL;
    if (!iter)
        return NULL;
    ydb = (struct ynode *)ytree_data(iter->cur);
    if (!ydb)
        return NULL;
    if (ydb->parent)
    {
        ydb = ydb->parent;
        if (ydb->parent)
        {
            iter->cur = ytree_find(ydb->parent->children, ydb->key);
            return ydb->key;
        }
        else
        {
            return NULL;
        }
    }
    return NULL;
}

const char *ymldb_iterator_next(struct ymldb_iterator *iter)
{
    struct ynode *ydb = NULL;
    ytree_iter *next = NULL;
    if (!iter)
        return NULL;
    ydb = (struct ynode *)ytree_data(iter->cur);
    if (!ydb)
        return NULL;

    if (ydb->parent)
    {
        next = ytree_next(ydb->parent->children, iter->cur);
        if (next)
        {
            iter->cur = next;
            ydb = (struct ynode *)ytree_data(iter->cur);
            return ydb->key;
        }
        return NULL;
    }
    return NULL;
}

const char *ymldb_iterator_prev(struct ymldb_iterator *iter)
{
    struct ynode *ydb = NULL;
    ytree_iter *prev = NULL;
    if (!iter)
        return NULL;
    ydb = (struct ynode *)ytree_data(iter->cur);
    if (!ydb)
        return NULL;

    if (ydb->parent)
    {
        prev = ytree_prev(ydb->parent->children, iter->cur);
        if (prev)
        {
            iter->cur = prev;
            ydb = (struct ynode *)ytree_data(iter->cur);
            return ydb->key;
        }
        return NULL;
    }
    return NULL;
}

const char *ymldb_iterator_get_value(struct ymldb_iterator *iter)
{
    struct ynode *ydb = NULL;
    if (!iter)
        return NULL;
    ydb = (struct ynode *)ytree_data(iter->cur);
    if (!ydb)
        return NULL;
    if (ydb->type == YMLDB_LEAF)
        return ydb->value;
    else if (ydb->type == YMLDB_LEAFLIST)
        return ydb->key;
    return NULL;
}

const char *ymldb_iterator_get_key(struct ymldb_iterator *iter)
{
    struct ynode *ydb = NULL;
    if (!iter)
        return NULL;
    ydb = (struct ynode *)ytree_data(iter->cur);
    if (!ydb)
        return NULL;
    return ydb->key;
}

int ymldb_file_push(char *filename, char *format, ...)
{
    int res = 0;
    FILE *outstream;
    struct ystream *input = NULL;

    _log_entrance();
    if (!filename)
        return -1;
    if (ymldb_is_created(filename))
        return -2;
    outstream = fopen(filename, "w+");
    if (!outstream)
        return -3;
    res = ymldb_create(filename, YMLDB_FLAG_NONE);
    if (res < 0)
    {
        res = -4;
        goto _done;
    }

    _log_debug("\n");
    input = _ystream_open_to_write(0);
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

    _ystream_reopen_to_read(input);
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
    if (!filename)
        return -1;
    if (ymldb_is_created(filename))
        return -2;
    instream = fopen(filename, "r");
    if (!instream)
        return -3;
    res = ymldb_create(filename, YMLDB_FLAG_NONE);
    if (res < 0)
    {
        fclose(instream);
        return -4;
    }

    ymldb_run(filename, instream, NULL);
    fclose(instream);

    _log_debug("\n");
    input = _ystream_open_to_write(0);
    if (!input)
    {
        res = -5;
        goto _done;
    }
    output = _ystream_open_to_write(0);
    if (!output)
    {
        res = -6;
        goto _done;
    }

    _ymldb_fprintf_head(input->stream, YMLDB_OP_GET, 0);
    _ymldb_remove_specifiers(input->stream, format);
    _ymldb_fprintf_tail(input->stream);

    _ystream_reopen_to_read(input);
    if (!input->stream)
    {
        _log_error("fail to open ymldb stream");
        goto _done;
    }

    _log_debug("@@ input->len=%zd buf=\n%s\n", input->len, input->buf);
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
    // ymldb_dump(stdout, NULL);
    ymldb_destroy(filename);
    return res;
}