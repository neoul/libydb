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
#include "ymldb.h"

// for debug - start

#define _out(FP, ...)                 \
    do                                \
    {                                 \
        if (FP)                       \
            fprintf(FP, __VA_ARGS__); \
    } while (0)

int _ymldb_log_error_parser(yaml_parser_t *parser)
{
    /* Display a parser error message. */
    switch (parser->error)
    {
    case YAML_MEMORY_ERROR:
        _log_error("Memory error: Not enough memory for parsing\n");
        break;

    case YAML_READER_ERROR:
        if (parser->problem_value != -1)
        {
            _log_error("Reader error: %s: #%X at %zd\n", parser->problem,
                       parser->problem_value, parser->problem_offset);
        }
        else
        {
            _log_error("Reader error: %s at %lu\n", parser->problem,
                       parser->problem_offset);
        }
        break;

    case YAML_SCANNER_ERROR:
        if (parser->context)
        {
            _log_error("Scanner error: %s at line %lu, column %lu\n"
                       "%s at line %lu, column %lu\n",
                       parser->context,
                       parser->context_mark.line + 1, parser->context_mark.column + 1,
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        else
        {
            _log_error("Scanner error: %s at line %lu, column %lu\n",
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        break;

    case YAML_PARSER_ERROR:
        if (parser->context)
        {
            _log_error("Parser error: %s at line %lu, column %lu\n"
                       "%s at line %lu, column %lu\n",
                       parser->context,
                       parser->context_mark.line + 1, parser->context_mark.column + 1,
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        else
        {
            _log_error("Parser error: %s at line %lu, column %lu\n",
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        break;

    case YAML_COMPOSER_ERROR:
        if (parser->context)
        {
            _log_error("Composer error: %s at line %lu, column %lu\n"
                       "%s at line %lu, column %lu\n",
                       parser->context,
                       parser->context_mark.line + 1, parser->context_mark.column + 1,
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        else
        {
            _log_error("Composer error: %s at line %lu, column %lu\n",
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        break;

    default:
        /* Couldn't happen. */
        _log_error("Internal error\n");
        break;
    }
    return 0;
}

static int alloc_cnt = 0;
void *_malloc(size_t s)
{
    alloc_cnt++;
    return malloc(s);
}

char *_str_dup(char *src)
{
    alloc_cnt++;
    return strdup(src);
}

void _free(void *p)
{
    alloc_cnt--;
    free(p);
}

void print_alloc_cnt()
{
    _log_debug("\n  @@  alloc_cnt %d @@\n", alloc_cnt);
}

#define free _free
#define malloc _malloc
#define strdup _str_dup
// gcc ymldb.c -lyaml -lcprops -L/home/neoul/projects/c_study/cprops/.libs -I./ -g3 -Wall -o ymldb
// for debug - end

#define S10 "          "
static char *gSpace = S10 S10 S10 S10 S10 S10 S10 S10 S10 S10;

static struct ymldb *gYdb;

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
    dbgidx = (dbgidx + 1) % 4;
    str = dbgstr[dbgidx];
    for (; src[i] > 0; i++)
    {
        if (src[i] == '\n')
        {
            str[j] = '\\';
            str[j + 1] = 'n';
            j = j + 2;
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
    str[j] = 0;
    return str;
}

static char *_ymldb_opcode_str(int opcode)
{
    static char opstr[32];
    opstr[0] = 0;
    if (opcode & YMLDB_OP_GET)
        strcat(opstr, "GET|");
    if (opcode & YMLDB_OP_DELETE)
        strcat(opstr, "DELETE|");
    if (opcode & YMLDB_OP_MERGE)
        strcat(opstr, "MERGE|");
    if (opcode & YMLDB_OP_SUBSCRIBE)
        strcat(opstr, "SUBSCRIBE|");
    if (opcode & YMLDB_OP_UNSUBSCRIBE)
        strcat(opstr, "UNSUBSCRIBE|");
    if (opcode & YMLDB_OP_PUBLISH)
        strcat(opstr, "PUBLISH|");
    return opstr;
}

// yaml func
static void _yaml_document_free(yaml_document_t *document)
{
    if (!document)
        return;
    if (document->nodes.start)
    {
        yaml_document_delete(document);
    }
    free(document);
}

static void _yaml_parser_free(yaml_parser_t *parser)
{
    if (!parser)
        return;
    if (parser->raw_buffer.start || parser->error)
    {
        yaml_parser_delete(parser);
    }
    free(parser);
}

static yaml_document_t *_yaml_document_load(yaml_parser_t *parser)
{
    yaml_document_t *document;
    if (!parser)
    {
        _log_error("ymldb:document: invalid parser.\n");
        return NULL;
    }

    document = malloc(sizeof(yaml_document_t));
    if (!document)
    {
        _log_error("ymldb:document: memory alloc failed.\n");
        return NULL;
    }
    if (!yaml_parser_load(parser, document))
    {
        // [TBD]
        _ymldb_log_error_parser(parser);
        _yaml_document_free(document);
        // _yaml_parser_free(parser);
        return NULL;
    }
    return document;
}

static yaml_parser_t *_yaml_parser_init(FILE *instream)
{
    yaml_parser_t *parser;
    if (!instream)
    {
        _log_error("ymldb:parser: invalid instream.\n");
        return NULL;
    }
    parser = malloc(sizeof(yaml_parser_t));
    if (!parser)
    {
        _log_error("ymldb:parser: memory alloc failed.\n");
        return NULL;
    }
    if (!yaml_parser_initialize(parser))
    {
        _ymldb_log_error_parser(parser);
        _yaml_parser_free(parser);
        return NULL;
    }
    yaml_parser_set_input_file(parser, instream);
    return parser;
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

static int _ymldb_node_dump(void *n, void *dummy)
{
    cp_avlnode *node = n;
    struct ymldb *ydb = node->value;
    FILE *stream = dummy;
    ymldb_dump(stream, ydb, ydb->level, 0); // not print parents
    return 0;
}

void ymldb_dump(FILE *stream, struct ymldb *ydb, int print_level, int no_print_children)
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
        _out(stream, "%.*s- %s\n", (ydb->level - 1) * 2, gSpace, ydb->key);
    }
    else
    {
        // _log_debug("%.*s%s: %s\n", (ydb->level - 1) * 2, gSpace, ydb->key, ydb->value);
        //_out(stream, "%.*s%s: \"%s\"\n", (ydb->level - 1) * 2, gSpace, ydb->key, _ymldb_str_dump(ydb->value));
        _out(stream, "%.*s%s: %s\n", (ydb->level - 1) * 2, gSpace, ydb->key, _ymldb_str_dump(ydb->value));
    }
    return;
}

void ymldb_dump_start(FILE *stream, int opcode, int sequence)
{
    _out(stream, "# %d\n", sequence);

    // %TAG !merge! actusnetworks.com:op:
    if (opcode & YMLDB_OP_MERGE)
    {
        _out(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_MERGE, YMLDB_TAG_MERGE);
    }
    if (opcode & YMLDB_OP_DELETE)
    {
        _out(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_DELETE, YMLDB_TAG_DELETE);
    }
    if (opcode & YMLDB_OP_GET)
    {
        _out(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_GET, YMLDB_TAG_GET);
    }
    if (opcode & YMLDB_OP_SUBSCRIBE)
    {
        _out(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_SUBSCRIBE, YMLDB_TAG_SUBSCRIBE);
    }
    if (opcode & YMLDB_OP_UNSUBSCRIBE)
    {
        _out(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_UNSUBSCRIBE, YMLDB_TAG_UNSUBSCRIBE);
    }
    if (opcode & YMLDB_OP_PUBLISH)
    {
        _out(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_PUBLISH, YMLDB_TAG_PUBLISH);
    }
    _out(stream, "---\n");
}

void ymldb_dump_end(FILE *stream)
{
    _out(stream, "\n...\n\n");
}

static void _ymldb_outstream_deinit(struct ymldb_cb *cb)
{
    if (cb->outstream)
    {
        fclose(cb->outstream);
        cb->outstream = NULL;
    }
    if (cb->outbuf)
    {
        free(cb->outbuf);
        cb->outbuf = NULL;
    }
    cb->outlen = 0;
}

static int _ymldb_outstream_init(struct ymldb_cb *cb)
{
    if (cb->outstream)
    {
        fseek(cb->outstream, 0, SEEK_SET); // restart stream.
        ymldb_dump_start(cb->outstream, cb->opcode, cb->sequence);
        return 0;
    }
    cb->outbuf = malloc(YMLDB_STREAM_BUF_SIZE);
    if (!cb->outbuf)
    {
        _log_error("outbuf alloc failed.\n");
        return -1;
    }
    cb->outlen = YMLDB_STREAM_BUF_SIZE;
    cb->outstream = fmemopen(cb->outbuf, cb->outlen, "w");
    if (!cb->outstream)
    {
        free(cb->outbuf);
        cb->outbuf = NULL;
        cb->outlen = 0;
        _log_error("outstream assign failed.\n");
        return -1;
    }
    ymldb_dump_start(cb->outstream, cb->opcode, cb->sequence);
    return 0;
}

static int _ymldb_local_send(struct ymldb_cb *cb);
// Return 1 if outstream is flushed, otherwise 0.
static int _ymldb_outstream_flush(struct ymldb_cb *cb, int forced)
{
    if (cb->outstream)
    {
        if(forced)
            goto flushing;
        else if (ftell(cb->outstream) >= YMLDB_STREAM_THRESHOLD)
            goto flushing;
        else
            return 0;
    }
    return 0;

flushing:
    ymldb_dump_end(cb->outstream);
    fflush(cb->outstream); // write the stream to outbuf.
    cb->outbuf[ftell(cb->outstream)] = 0; // end of string.
    _log_debug("cb->outbuf - START\n%s", cb->outbuf);
    _log_debug("cb->outbuf - END\n");

    if(cb->flags & YMLDB_FLAG_LOCAL)
        _ymldb_local_send(cb);
    // if(flags & YMLDB_FLAG_CONN)
    //     ymldb_conn_send(cb);
    return 1;
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
        if (strcmp(ancestor1->key, ancestor2->key) != 0)
            break;
        print_level++;
    }
    return print_level;
}

void _ymldb_node_merge_notify(struct ymldb_cb *cb, struct ymldb *ydb)
{
    int flushed;
    int print_level = 0;
    if (!cb || !ydb)
        return;
    if (cb->last_notify)
        print_level = _ymldb_print_level(cb->last_notify, ydb);
    ymldb_dump(cb->outstream, ydb, print_level, 0);
    flushed = _ymldb_outstream_flush(cb, 0);
    if (flushed) {
        cb->last_notify = NULL;
        _ymldb_outstream_init(cb);
    }
    else
        cb->last_notify = ydb;
    return;
}

void _ymldb_node_delete_notify(struct ymldb_cb *cb, struct ymldb *parent, char *key)
{
    int flushed;
    int print_level = 0;
    struct ymldb *ydb = NULL;
    if (!cb || !parent || !key)
        return;
    if (parent->type != YMLDB_BRANCH)
        return;
    ydb = cp_avltree_get(parent->children, key);
    if (!ydb)
        return; // do nothing if not exist.

    if (cb->last_notify)
        print_level = _ymldb_print_level(cb->last_notify, ydb);
    ymldb_dump(cb->outstream, ydb, print_level, 1);
    flushed = _ymldb_outstream_flush(cb, 0);
    if (flushed) {
        cb->last_notify = NULL;
        _ymldb_outstream_init(cb);
    }
    else
        cb->last_notify = parent; // parent should be saved because of the ydb will be removed.
    return;
}

void _ymldb_node_get_reply(struct ymldb_cb *cb, struct ymldb *parent, char *key)
{
    int flushed;
    int print_level = 0;
    struct ymldb *ydb = NULL;
    if (!cb || !parent || !key)
        return;
    if (parent->type != YMLDB_BRANCH)
        return;
    ydb = cp_avltree_get(parent->children, key);
    if (!ydb)
        return; // do nothing if not exist.

    if (cb->last_notify)
        print_level = _ymldb_print_level(cb->last_notify, ydb);
    ymldb_dump(cb->outstream, ydb, print_level, 0);
    flushed = _ymldb_outstream_flush(cb, 0);
    if (flushed) {
        cb->last_notify = NULL;
        _ymldb_outstream_init(cb);
    }
    else
        cb->last_notify = ydb;
    return;
}

struct ymldb *_ymldb_node_merge(struct ymldb_cb *cb, struct ymldb *parent,
                                ymldb_type_t type, char *key, char *value)
{
    struct ymldb *ydb = NULL;
    char *ykey = NULL;
    if (parent)
    {
        if (parent->type != YMLDB_BRANCH)
        {
            _log_error("Unable to assign new ymldb to a leaf ymldb node.\n");
            _log_error("- parent ymldb: %s, child ymldb %s\n", parent->key, key);
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
                    _ymldb_node_merge_notify(cb, ydb);
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
    _ymldb_node_merge_notify(cb, ydb);
    return ydb;

free_ydb:
    _log_error("Memory error: Not enough memory for ymldb node\n");
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

void _ymldb_node_delete(struct ymldb_cb *cb, struct ymldb *parent, char *key)
{
    if (parent)
    {
        struct ymldb *ydb = NULL;
        if (parent->type != YMLDB_BRANCH)
        {
            _log_error("Unable to delete a node from the leaf.\n");
            _log_error("- parent ymldb: %s, child ymldb %s\n", parent->key, key);
            return;
        }
        _ymldb_node_delete_notify(cb, parent, key);
        ydb = cp_avltree_delete(parent->children, key);
        _ymldb_node_free(ydb);
    }
    return;
}

void _ymldb_node_get(struct ymldb_cb *cb, struct ymldb *parent, char *key)
{
    if (parent)
    {
        // struct ymldb *ydb = NULL;
        if (parent->type != YMLDB_BRANCH)
        {
            _log_error("Unable to delete a node from the leaf.\n");
            _log_error("- parent ymldb: %s, child ymldb %s\n", parent->key, key);
            return;
        }
        _ymldb_node_get_reply(cb, parent, key);
    }
    return;
}

int _ymldb_merge(struct ymldb_cb *cb, struct ymldb *p_ydb, int index, int p_index)
{
    yaml_node_t *node = NULL;
    node = yaml_document_get_node(cb->document, index);
    if (!p_ydb)
        return -1;

    if (!node)
        return 0;

    if (p_ydb->level == 1)
    {
        if (strcmp(cb->key, p_ydb->key) != 0)
        {
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
            yaml_node_t *node = yaml_document_get_node(cb->document, *item);
            char *key = (char *)node->data.scalar.value;
            if (node->type == YAML_SCALAR_NODE)
            {
                _ymldb_node_merge(cb, p_ydb, YMLDB_LEAFLIST, key, NULL);
            }
            else
            {
                _ymldb_merge(cb, p_ydb, *item, index);
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
            yaml_node_t *key_node = yaml_document_get_node(cb->document, pair->key);
            yaml_node_t *value_node = yaml_document_get_node(cb->document, pair->value);
            char *key = (char *)key_node->data.scalar.value;
            char *value = (char *)value_node->data.scalar.value;

            if (value_node->type == YAML_SCALAR_NODE)
            {
                _ymldb_node_merge(cb, p_ydb, YMLDB_LEAF, key, value);
            }
            else
            { // not leaf
                struct ymldb *ydb = NULL;
                // _log_debug("key %s, value -\n", key);
                ydb = _ymldb_node_merge(cb, p_ydb, YMLDB_BRANCH, key, NULL);
                _ymldb_merge(cb, ydb, pair->value, index);
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

int _ymldb_delete(struct ymldb_cb *cb, struct ymldb *p_ydb, int index, int p_index)
{
    yaml_node_t *node = NULL;
    if (!p_ydb)
        return -1;
    node = yaml_document_get_node(cb->document, index);
    if (!node)
        return 0;

    if (p_ydb->level == 1)
    {
        if (strcmp(cb->key, p_ydb->key) != 0)
        {
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
            yaml_node_t *node = yaml_document_get_node(cb->document, *item);
            char *key = (char *)node->data.scalar.value;
            if (node->type == YAML_SCALAR_NODE)
            {
                _ymldb_node_delete(cb, p_ydb, key);
            }
            else
            {
                _ymldb_delete(cb, p_ydb, *item, index);
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
            yaml_node_t *key_node = yaml_document_get_node(cb->document, pair->key);
            yaml_node_t *value_node = yaml_document_get_node(cb->document, pair->value);
            char *key = (char *)key_node->data.scalar.value;

            if (value_node->type == YAML_SCALAR_NODE)
            {
                _ymldb_node_delete(cb, p_ydb, key);
            }
            else
            { // not leaf
                struct ymldb *ydb = NULL;
                // _log_debug("key %s, value -\n", key);
                ydb = cp_avltree_get(p_ydb->children, key);
                _ymldb_delete(cb, ydb, pair->value, index);
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

int _ymldb_get(struct ymldb_cb *cb, struct ymldb *p_ydb, int index, int p_index)
{
    yaml_node_t *node = NULL;
    if (!p_ydb)
        return -1;
    node = yaml_document_get_node(cb->document, index);
    if (!node)
        return 0;

    switch (node->type)
    {
    case YAML_SEQUENCE_NODE:
    {
        yaml_node_item_t *item;
        // printf("SEQ c=%d p=%d\n", index, p_index);
        for (item = node->data.sequence.items.start;
             item < node->data.sequence.items.top; item++)
        {
            yaml_node_t *node = yaml_document_get_node(cb->document, *item);
            char *key = (char *)node->data.scalar.value;
            if (node->type == YAML_SCALAR_NODE)
            {
                _ymldb_node_get(cb, p_ydb, key);
            }
            else
            {
                _ymldb_get(cb, p_ydb, *item, index);
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
            yaml_node_t *key_node = yaml_document_get_node(cb->document, pair->key);
            yaml_node_t *value_node = yaml_document_get_node(cb->document, pair->value);
            char *key = (char *)key_node->data.scalar.value;

            if (value_node->type == YAML_SCALAR_NODE)
            {
                _ymldb_node_get(cb, p_ydb, key);
            }
            else
            { // not leaf
                struct ymldb *ydb = NULL;
                // _log_debug("key %s\n", key);
                ydb = cp_avltree_get(p_ydb->children, key);
                _ymldb_get(cb, ydb, pair->value, index);
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

int _ymldb_op_extract(struct ymldb_cb *cb)
{
    // char *op = YMLDB_OP_MERGE;
    int opcode = 0;
    if (!cb || !cb->document)
        return opcode;
    if (cb->document->tag_directives.start != cb->document->tag_directives.end)
    {
        char *op;
        yaml_tag_directive_t *tag;
        for (tag = cb->document->tag_directives.start;
             tag != cb->document->tag_directives.end; tag++)
        {
            op = (char *)tag->handle;
            if (strcmp(op, YMLDB_TAG_OP_MERGE) == 0)
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
            else if (strcmp(op, YMLDB_TAG_OP_SUBSCRIBE) == 0)
            {
                opcode = opcode | YMLDB_OP_SUBSCRIBE;
            }
            else if (strcmp(op, YMLDB_TAG_OP_UNSUBSCRIBE) == 0)
            {
                opcode = opcode | YMLDB_OP_UNSUBSCRIBE;
            }
            else if (strcmp(op, YMLDB_TAG_OP_PUBLISH) == 0)
            {
                opcode = opcode | YMLDB_OP_PUBLISH;
            }
        }
    }
    return opcode;
}

int _ymldb_run(struct ymldb_cb *cb, FILE *instream)
{
    int done = 0;
    yaml_parser_t *parser;

    if (!cb)
    {
        _log_error("ymldb:construct:invalid ymldb_cb\n");
        return -1;
    }
    if (!instream)
    {
        _log_error("ymldb:construct:invalid input stream\n");
        return -1;
    }

    parser = _yaml_parser_init(instream);
    if (!parser)
    {
        _log_error("ymldb:construct:parser init failed\n");
        return -1;
    }

    _log_debug("> start\n");
    while (!done)
    {
        yaml_node_t *yroot = NULL;
        /* Get the next ymldb document. */
        cb->document = _yaml_document_load(parser);
        if (!cb->document)
        {
            _log_error("ymldb:construct:load a document from parser - failed\n");
            break;
        }

        cb->opcode = _ymldb_op_extract(cb);

        yroot = yaml_document_get_root_node(cb->document);
        if (yroot)
        {
            cb->sequence++;
            _log_debug("%dth OPCODE=(%s)\n", cb->sequence, _ymldb_opcode_str(cb->opcode));

            _ymldb_outstream_init(cb);
            if (cb->opcode & YMLDB_OP_PUBLISH);
            if (cb->opcode & YMLDB_OP_SUBSCRIBE);
            if (cb->opcode & YMLDB_OP_UNSUBSCRIBE);

            if (cb->opcode & YMLDB_OP_MERGE)
                _ymldb_merge(cb, gYdb, 1, 1);
            if (cb->opcode & YMLDB_OP_DELETE)
                _ymldb_delete(cb, gYdb, 1, 1);
            if (cb->opcode & YMLDB_OP_GET)
                _ymldb_get(cb, gYdb, 1, 1);
            _ymldb_outstream_flush(cb, 1); // forced flush!
        }
        else
        {
            done = 1;
        }
        _yaml_document_free(cb->document);
        cb->document = NULL;
    }
    _log_debug("> end\n\n");
    _ymldb_outstream_deinit(cb);
    _yaml_parser_free(parser);
    return 0;
}

int ymldb_run(struct ymldb_cb *cb, int fd)
{
    int res;
    FILE *instream = _ymldb_fopen_from_fd(fd, "r");
    res = _ymldb_run(cb, instream);
    fclose(instream);
    return res;
}

struct ymldb_cb *ymldb_create(char *key, unsigned int flags, int option)
{
    struct ymldb_cb *cb;
    if (!key)
    {
        _log_error("ymldb:init: no key\n");
    }

    // init top
    if (!gYdb)
    {
        gYdb = _ymldb_node_merge(NULL, NULL, YMLDB_BRANCH, "top", NULL);
        if (!gYdb)
        {
            _log_error("ymldb:init:top failed.\n");
            return NULL;
        }
    }
    if (cp_avltree_get(gYdb->children, key))
    {
        _log_error("ymldb:init: key exsits.\n");
        return NULL;
    }

    cb = malloc(sizeof(struct ymldb_cb));
    if (!cb)
    {
        _log_error("ymldb:init: alloc failed.\n");
        return NULL;
    }
    cb->key = strdup(key);
    if (!cb->key)
    {
        _log_error("ymldb:init: key alloc failed.\n");
        free(cb);
        return NULL;
    }

    cb->document = NULL;
    cb->flags = flags &(YMLDB_FLAG_LOCAL | YMLDB_FLAG_PUBLISHER | YMLDB_FLAG_SUBSCRIBER);
    cb->outstream = NULL;

    cb->ydb = _ymldb_node_merge(cb, gYdb, YMLDB_BRANCH, key, NULL);
    if (!cb->ydb)
    {
        _log_error("ymldb:init: init failed.\n");
        free(cb->key);
        free(cb);
        return NULL;
    }
    cb->last_notify = NULL;

    cb->fd_local = 0;
    cb->fd_publisher = 0;
    memset(cb->fd_subscriber, 0x0, sizeof(cb->fd_subscriber));
    
    flags = flags &(YMLDB_FLAG_LOCAL | YMLDB_FLAG_PUBLISHER | YMLDB_FLAG_SUBSCRIBER);
    if(flags & YMLDB_FLAG_PUBLISHER || flags & YMLDB_FLAG_SUBSCRIBER)
        ymldb_conn_init(cb, flags);
    if(flags & YMLDB_FLAG_LOCAL)
        ymldb_local_init(cb, option); // option is fd if YMLDB_FLAG_LOCAL.
    return cb;
}

int ymldb_conn_deinit(struct ymldb_cb *cb)
{
    if (!cb)
    {
        _log_error("ymldb:fd: no cb\n");
        return -1;
    }
    if (cb->fd_publisher)
    {
        close(cb->fd_publisher);
        cb->fd_publisher = 0;
    }
    if (cb->flags & YMLDB_FLAG_PUBLISHER)
    {
        for (int i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
        {
            if (cb->fd_subscriber[i])
            {
                close(cb->fd_subscriber[i]);
                cb->fd_subscriber[i] = 0;
            }
        }
    }
    cb->flags = cb->flags & (~YMLDB_FLAG_CONN);
    cb->flags = cb->flags & (~YMLDB_FLAG_PUBLISHER);
    cb->flags = cb->flags & (~YMLDB_FLAG_SUBSCRIBER);
    cb->flags = cb->flags & (~YMLDB_FLAG_RECONNECT);
    return 0;
}

int ymldb_conn_init(struct ymldb_cb *cb, int flags)
{
    int fd;
    char socketpath[128];
    struct sockaddr_un addr;
    if (!cb)
    {
        _log_error("ymldb:fd: no cb\n");
        return -1;
    }

    if (cb->flags & YMLDB_FLAG_CONN)
    {
        ymldb_conn_deinit(cb);
    }

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        _log_error("ymldb:fd: socket failed (%s).\n", strerror(errno));
        return -1;
    }

    snprintf(socketpath, sizeof(socketpath), YMLDB_UNIXSOCK_PATH, cb->key);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketpath, sizeof(addr.sun_path) - 1);
    addr.sun_path[0] = 0;
    cb->fd_publisher = fd;

    if (flags & YMLDB_FLAG_PUBLISHER)
    { // fd_publisher
        cb->flags |= YMLDB_FLAG_PUBLISHER;
        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            _log_error("ymldb:fd: bind failed (%s).\n", strerror(errno));
            cb->flags |= YMLDB_FLAG_RECONNECT;
            return -1;
        }
        if (listen(fd, YMLDB_SUBSCRIBER_MAX) < 0)
        {
            _log_error("ymldb:fd: listen failed (%s).\n", strerror(errno));
            cb->flags |= YMLDB_FLAG_RECONNECT;
            return -1;
        }
    }
    else if(flags & YMLDB_FLAG_SUBSCRIBER)
    { // SUBSCRIBER
        cb->flags |= YMLDB_FLAG_SUBSCRIBER;
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        {
            _log_error("ymldb:fd: connect failed (%s).\n", strerror(errno));
            cb->flags |= YMLDB_FLAG_RECONNECT;
            return -1;
        }
    }
    return fd;
}

int ymldb_conn_set(struct ymldb_cb *cb, fd_set *set)
{
    int max = 0;
    if (!cb || !set)
    {
        _log_error("ymldb:fd: no cb or set\n");
        return 0;
    }
    if (cb->flags & YMLDB_FLAG_CONN)
    {
        if (cb->fd_publisher)
        {
            FD_SET(cb->fd_publisher, set);
            max = cb->fd_publisher > max ? cb->fd_publisher : max;
        }
        if (cb->flags & YMLDB_FLAG_PUBLISHER)
        {
            for (int i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
            {
                if (cb->fd_subscriber[i])
                {
                    FD_SET(cb->fd_subscriber[i], set);
                    max = cb->fd_subscriber[i] > max ? cb->fd_subscriber[i] : max;
                }
            }
        }
    }
    return max;
}

int ymldb_conn_run(struct ymldb_cb *cb, fd_set *set)
{
    if (!cb || !set)
    {
        _log_error("ymldb:fd: no cb or set\n");
        return -1;
    }

    if (!(cb->flags & YMLDB_FLAG_CONN))
    {
        _log_error("ymldb:fd: no required publishing.\n");
        return -1;
    }

    // check reconnect required.
    if (cb->flags & YMLDB_FLAG_RECONNECT)
    {
        return ymldb_conn_init(cb, cb->flags & YMLDB_FLAG_PUBLISHER);
    }

    _log_debug("%s:%d\n", __FUNCTION__, __LINE__);
    if (cb->fd_publisher)
    {
        _log_debug("%s:%d\n", __FUNCTION__, __LINE__);
        if (FD_ISSET(cb->fd_publisher, set))
        {
            _log_debug("%s:%d\n", __FUNCTION__, __LINE__);
            if (cb->flags & YMLDB_FLAG_PUBLISHER)
            {
                _log_debug("%s:%d\n", __FUNCTION__, __LINE__);
                int i, fd;
                fd = accept(cb->fd_publisher, NULL, NULL);
                if (fd < 0)
                {
                    _log_error("accept failed (%s)\n", strerror(errno));
                    return -1;
                }
                for (i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
                {
                    if (cb->fd_subscriber[i] <= 0)
                    {
                        cb->fd_subscriber[i] = fd;
                        break;
                    }
                }
                if (i >= YMLDB_SUBSCRIBER_MAX)
                {
                    _log_error("subscription over..\n");
                }
                FD_CLR(cb->fd_publisher, set);
            }
            else
            { // for fd_subscriber
                _log_debug("%s:%d\n", __FUNCTION__, __LINE__);
                int readlen;
                char readbuf[512];
                // memset(readbuf, 0, sizeof(readbuf));
                readlen = read(cb->fd_publisher, readbuf, sizeof(readbuf));
                if (readlen < 0)
                {
                    cb->flags |= YMLDB_FLAG_RECONNECT;
                    return -1;
                }
                readbuf[readlen] = 0;
                printf("readlen %d readbuf %s", readlen, readbuf);
                FD_CLR(cb->fd_publisher, set);

                ymldb_run_with_string(cb, readbuf);
            }
        }
    }
    if (cb->flags & YMLDB_FLAG_PUBLISHER)
    {
        for (int i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
        {
            if (FD_ISSET(cb->fd_subscriber[i], set))
            {
                _log_debug("%s:%d\n", __FUNCTION__, __LINE__);
                int readlen;
                char readbuf[512];
                readlen = read(cb->fd_subscriber[i], readbuf, sizeof(readbuf));
                if (readlen < 0)
                {
                    close(cb->fd_subscriber[i]);
                    cb->fd_subscriber[i] = 0;
                    continue;
                }
                readbuf[readlen] = 0;
                printf("readlen %d readbuf %s", readlen, readbuf);
                FD_CLR(cb->fd_subscriber[i], set);
                ymldb_run_with_string(cb, readbuf);
            }
        }
    }
    return 0;
}

int ymldb_local_init(struct ymldb_cb *cb, int fd)
{
    if (!cb)
    {
        _log_error("no cb\n");
        return -1;
    }
    if(fd <= 0) {
        _log_error("fd_local: failed due to fd <= %d\n", fd);
        return -1;
    }
    cb->fd_local = fd;
    cb->flags |= YMLDB_FLAG_LOCAL;
    _log_debug("fd_local: init to %d.\n", cb->fd_local);
    return 0;
}

int ymldb_local_deinit(struct ymldb_cb *cb)
{
    if (!cb)
    {
        _log_error("no cb\n");
        return -1;
    }
    cb->fd_local = 0;
    cb->flags = cb->flags & (~YMLDB_FLAG_LOCAL);
    _log_debug("fd_local: deinit.\n");
    return 0;
}

static int _ymldb_local_send(struct ymldb_cb *cb)
{
    int res = 0;
    int sent = 0;
    int retry = 0;
rewrite:
    if(cb->fd_local <= 0) return -1;
    res = write(cb->fd_local, cb->outbuf, ftell(cb->outstream));
    if(res < 0) {
        _log_error("fd_local: send failed to fd_local(%d) (%s)\n", cb->fd_local, strerror(errno));
        ymldb_local_deinit(cb);
        return -1;
    }
    sent = res + sent;
    if(sent < ftell(cb->outstream) && retry < 3) {
        retry++;
        goto rewrite;
    }
    return 0;
}

void ymldb_destroy(struct ymldb_cb *cb)
{
    if (!cb)
        return;
    if (cb->ydb)
        _ymldb_node_free( cb->ydb);
    if (cb->document)
        _yaml_document_free(cb->document);
    _ymldb_outstream_deinit(cb);
    ymldb_conn_deinit(cb);
    if (cb->key)
        free(cb->key);
    if (cb->outstream)
        fclose(cb->outstream);
    free(cb);
    // [FIXME] how to remove gYdb?
}

int ymldb_run_with_string(struct ymldb_cb *cb, char *ymldb_data)
{
    FILE *instream;
    _log_debug("ymldb_data=\n");
    _log_debug("\n'%s'\n", ymldb_data);
    if (!cb)
    {
        _log_error("no cb\n");
    }
    instream = fmemopen(ymldb_data, strlen(ymldb_data), "r");
    if (instream)
    {
        _ymldb_run(cb, instream);
        fclose(instream);
    }
    return 0;
}

int ymldb_push(struct ymldb_cb *cb, int opcode, char *format, ...)
{
    FILE *stream;
    char streambuf[512] = {
        0,
    };
    if (!cb || opcode == 0)
    {
        _log_error("ymldb:push: no cb or opcode\n");
        return -1;
    }

    stream = fmemopen(streambuf, sizeof(streambuf), "w");
    if (!stream)
    {
        _log_error("ymldb:push: unable fmemopen stream\n");
        return -1;
    }

    // write ymldb data to the streambuf
    ymldb_dump_start(stream, opcode, 0);
    va_list args;
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);
    ymldb_dump_end(stream);
    fclose(stream);
    // read ymldb data from the streambuf
    return ymldb_run_with_string(cb, streambuf);
}

int ymldb_pull(struct ymldb_cb *cb, char *format, ...)
{
    int opcode = YMLDB_OP_GET;
    FILE *stream;
    char streambuf[512];
    streambuf[0] = 0;
    if (!cb || opcode == 0)
    {
        _log_error("ymldb:pull: no cb or opcode\n");
        return -1;
    }

    stream = fmemopen(streambuf, sizeof(streambuf), "w");
    if (!stream)
    {
        _log_error("ymldb:pull: unable fmemopen stream\n");
        return -1;
    }
    // write ymldb data to the streambuf
    ymldb_dump_start(stream, opcode, 0);
    va_list args;
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);
    ymldb_dump_end(stream);
    fclose(stream);
    // read ymldb data from the streambuf
    ymldb_run_with_string(cb, streambuf);
    return 0;
}

// write a key and value
int _ymldb_write(struct ymldb_cb *cb, int opcode, int num, ...)
{
    int i;
    FILE *stream;
    char yml_data[256] = {
        0,
    };
    va_list args;
    if (!cb || opcode == 0)
    {
        _log_error("ymldb:write: no cb or opcode\n");
        return -1;
    }

    stream = fmemopen(yml_data, sizeof(yml_data), "w");
    if (!stream)
    {
        _log_error("ymldb:write: unable fmemopen stream\n");
        return -1;
    }
    ymldb_dump_start(stream, opcode, 0);
    va_start(args, num);
    for (i = 0; i < num; i++)
    {
        char *token = va_arg(args, char *);
        if (!token)
        {
            fprintf(stream, "\n");
            break;
        }
        if (i + 1 < num)
            fprintf(stream, "%.*s%s:\n", i * 2, gSpace, token);
        else
            fprintf(stream, "%.*s%s\n", i * 2, gSpace, token);
    }
    va_end(args);
    ymldb_dump_end(stream);
    fclose(stream);
    return ymldb_run_with_string(cb, yml_data);
}

// read a value by the key.
char *ymldb_read(struct ymldb_cb *cb, int num, ...)
{
    return NULL;
}

int main(int argc, char *argv[])
{
    int help = 0;
    int infile = 0;
    int outfile = 0;
    int k;
    char *infilename = "";
    char *outfilename = "";
    int infd = 0;
    int outfd = 0;

    /* Analyze command line options. */
    for (k = 1; k < argc; k++)
    {
        // fprintf(stdout, "argv[%d]=%s\n", k, argv[k]);
        if (strcmp(argv[k], "-h") == 0 || strcmp(argv[k], "--help") == 0)
        {
            help = 1;
        }
        else if (strcmp(argv[k], "-i") == 0 || strcmp(argv[k], "--infile") == 0)
        {
            infile = k;
        }
        else if (strcmp(argv[k], "-o") == 0 || strcmp(argv[k], "--outfile") == 0)
        {
            outfile = k;
        }
        else
        {
            if (infile + 1 == k)
            {
                infilename = argv[k];
                fprintf(stderr, "infilename %s\n", infilename);
            }
            else if (outfile + 1 == k)
            {
                outfilename = argv[k];
                fprintf(stderr, "outfilename %s\n", outfilename);
            }
            else
            {
                fprintf(stderr, "Unrecognized option: %s\n"
                                "Try `%s --help` for more information.\n",
                        argv[k], argv[0]);
                return 1;
            }
        }
    }

    /* Display the help string. */
    if (help)
    {
        printf("%s <input\n"
               "or\n%s -h | --help\nDeconstruct a YAML in\n\nOptions:\n"
               "-h, --help\t\tdisplay this help and exit\n"
               "-i, --infile\t\tinput ymldb file to read. (*.yml)\n"
               "-o, --outfile\t\toutput ymldb file to read. (*.yml)\n",
               argv[0], argv[0]);
        return 0;
    }

    infd = STDIN_FILENO;
    if (strlen(infilename) > 0)
    {
        infd = open(infilename, O_RDONLY, 0644);
        if (infd < 0)
        {
            fprintf(stderr, "file open error. %s\n", strerror(errno));
            return 1;
        }
    }

    outfd = STDOUT_FILENO;
    if (strlen(outfilename) > 0)
    {
        outfd = open(outfilename, O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (outfd < 0)
        {
            fprintf(stderr, "outfilename file open error. %s\n", strerror(errno));
            return 1;
        }
    }

    struct ymldb_cb *cb;
    cb = ymldb_create("interface", YMLDB_FLAG_LOCAL, outfd);
    if (!cb)
        return -1;

    ymldb_run(cb, infd);
    
    // // ymldb_push(cb, YMLDB_OP_MERGE,
    // //            "system:\n"
    // //            "  product: %s\n"
    // //            "  serial-number: %s\n",
    // //            "G.FAST-HN5124D",
    // //            "HN5124-S100213124");
    // // ymldb_write(cb, 3, "system", "product", "abc");

    ymldb_dump(stdout, gYdb, 0, 0);

    // char productstr[32];
    // char serial_number[32];
    // ymldb_pull(cb,
    //            "system:\n"
    //            "  serial-number: %s\n"
    //            "  product: %s\n",
    //            productstr,
    //            serial_number);

    ymldb_destroy(cb);

    // for debug
    _log_debug("\n");
    _log_debug("  alloc_cnt %d\n", alloc_cnt);

    if (infd)
        close(infd);
    if (outfd)
        close(outfd);

    return 0;
}