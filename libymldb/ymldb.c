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

#define _out(FP, ...) fprintf(FP, __VA_ARGS__)

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
            _log_error("reader error: %s at %lu\n", parser->problem,
                       parser->problem_offset);
        }
        break;

    case YAML_SCANNER_ERROR:
        if (parser->context)
        {
            _log_error("scanner error: %s at line %lu, column %lu\n"
                       "%s at line %lu, column %lu\n",
                       parser->context,
                       parser->context_mark.line + 1, parser->context_mark.column + 1,
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        else
        {
            _log_error("scanner error: %s at line %lu, column %lu\n",
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        break;

    case YAML_PARSER_ERROR:
        if (parser->context)
        {
            _log_error("parser error: %s at line %lu, column %lu\n"
                       "%s at line %lu, column %lu\n",
                       parser->context,
                       parser->context_mark.line + 1, parser->context_mark.column + 1,
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        else
        {
            _log_error("parser error: %s at line %lu, column %lu\n",
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        break;

    case YAML_COMPOSER_ERROR:
        if (parser->context)
        {
            _log_error("composer error: %s at line %lu, column %lu\n"
                       "%s at line %lu, column %lu\n",
                       parser->context,
                       parser->context_mark.line + 1, parser->context_mark.column + 1,
                       parser->problem, parser->problem_mark.line + 1,
                       parser->problem_mark.column + 1);
        }
        else
        {
            _log_error("composer error: %s at line %lu, column %lu\n",
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

#define free _free
#define malloc _malloc
#define strdup _str_dup
// gcc ymldb.c -lyaml -lcprops -L/home/neoul/projects/c_study/cprops/.libs -I./ -g3 -Wall -o ymldb
// for debug - end

#define S10 "          "
static char *gSpace = S10 S10 S10 S10 S10 S10 S10 S10 S10 S10;

static struct ymldb *gYdb = NULL;
static cp_avltree *gYcb = NULL;
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
    static char opstr[128];
    opstr[0] = 0;
    strcat(opstr, "(");
    if (opcode & YMLDB_OP_SEQ)
        strcat(opstr, "seq|");
    if (opcode & YMLDB_OP_GET)
        strcat(opstr, "get|");
    if (opcode & YMLDB_OP_DELETE)
        strcat(opstr, "delete|");
    if (opcode & YMLDB_OP_MERGE)
        strcat(opstr, "merge|");
    if (opcode & YMLDB_OP_SUBSCRIBER)
        strcat(opstr, "subscriber|");
    if (opcode & YMLDB_OP_PUBLISHER)
        strcat(opstr, "publisher|");
    if (opcode & YMLDB_OP_SYNC)
        strcat(opstr, "sync|");
    strcat(opstr, ")");
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
        _log_error("invalid parser.\n");
        return NULL;
    }

    document = malloc(sizeof(yaml_document_t));
    if (!document)
    {
        _log_error("memory alloc failed.\n");
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
        _log_error("invalid instream.\n");
        return NULL;
    }
    parser = malloc(sizeof(yaml_parser_t));
    if (!parser)
    {
        _log_error("memory alloc failed.\n");
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

void ymldb_dump_start(FILE *stream, unsigned int opcode, unsigned int sequence)
{
    // fseek(stream, 0, SEEK_SET);
    _out(stream, "# %u\n", sequence);

    // %TAG !merge! actusnetworks.com:op:
    if(opcode & YMLDB_OP_SEQ)
    {
        _out(stream, "%s %s %s%u\n", "%TAG", YMLDB_TAG_OP_SEQ, YMLDB_TAG_SEQ, sequence);
    }
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
    if (opcode & YMLDB_OP_SUBSCRIBER)
    {
        _out(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_SUBSCRIBER, YMLDB_TAG_SUBSCRIBER);
    }
    if (opcode & YMLDB_OP_SYNC)
    {
        _out(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_SYNC, YMLDB_TAG_SYNC);
    }
    if (opcode & YMLDB_OP_PUBLISHER)
    {
        _out(stream, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_PUBLISHER, YMLDB_TAG_PUBLISHER);
    }
    _out(stream, "---\n");
}

void ymldb_dump_end(FILE *stream)
{
    // fflush(stream);
    _out(stream, "\n...\n\n");
}

static void _ymldb_reply_deinit(struct ymldb_cb *cb)
{
    if (cb->reply.stream)
    {
        fclose(cb->reply.stream);
        cb->reply.stream = NULL;
    }
    if (cb->reply.buf)
    {
        free(cb->reply.buf);
        cb->reply.buf = NULL;
    }
    cb->reply.buflen = 0;
}

static int _ymldb_reply_init(struct ymldb_cb *cb)
{
    if (cb->reply.stream)
    {
        fseek(cb->reply.stream, 0, SEEK_SET); // restart stream.
        ymldb_dump_start(cb->reply.stream, cb->opcode, cb->sequence);
        return 0;
    }
    cb->reply.buf = malloc(YMLDB_STREAM_BUF_SIZE);
    if (!cb->reply.buf)
    {
        _log_error("reply.buf alloc failed.\n");
        return -1;
    }
    cb->reply.buflen = YMLDB_STREAM_BUF_SIZE;
    cb->reply.stream = fmemopen(cb->reply.buf, cb->reply.buflen, "w");
    if (!cb->reply.stream)
    {
        free(cb->reply.buf);
        cb->reply.buf = NULL;
        cb->reply.buflen = 0;
        _log_error("reply.stream assign failed.\n");
        return -1;
    }
    ymldb_dump_start(cb->reply.stream, cb->opcode, cb->sequence);
    return 0;
}

// static int _ymldb_local_send(struct ymldb_cb *cb);
static int _ymldb_conn_send(struct ymldb_cb *cb);
// Return 1 if reply.stream is flushed, otherwise 0.
static int _ymldb_reply_flush(struct ymldb_cb *cb, int forced)
{
    if (cb->reply.stream)
    {
        if (forced)
            goto flushing;
        else if (ftell(cb->reply.stream) >= YMLDB_STREAM_THRESHOLD)
            goto flushing;
        else
            return 0;
    }
    return 0;

flushing:
    ymldb_dump_end(cb->reply.stream);
    fflush(cb->reply.stream);                   // write the stream to reply.buf.
    cb->reply.buf[ftell(cb->reply.stream)] = 0; // end of string.
    // _log_debug("cb->reply.buf - START\n%s", cb->reply.buf);
    // _log_debug("cb->reply.buf - END\n");

    if(cb->out.stream) {
        fputs(cb->reply.buf, cb->out.stream);
        fflush(cb->out.stream);
    }

    // if (cb->flags & YMLDB_FLAG_LOCAL)
    //     _ymldb_local_send(cb);
    if (cb->flags & YMLDB_FLAG_CONN)
        _ymldb_conn_send(cb);
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

void _ymldb_node_merge_reply(struct ymldb_cb *cb, struct ymldb *ydb)
{
    int flushed;
    int print_level = 0;
    if (!cb || !ydb)
        return;
    if (cb->last_notify)
        print_level = _ymldb_print_level(cb->last_notify, ydb);
    if (cb->reply.stream)
        ymldb_dump(cb->reply.stream, ydb, print_level, 0);
    flushed = _ymldb_reply_flush(cb, 0);
    if (flushed)
    {
        cb->last_notify = NULL;
        _ymldb_reply_init(cb);
    }
    else
        cb->last_notify = ydb;
    return;
}

void _ymldb_node_get_reply(struct ymldb_cb *cb, struct ymldb *parent, char *key)
{

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
                    _ymldb_node_merge_reply(cb, ydb);
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
    _ymldb_node_merge_reply(cb, ydb);
    return ydb;

free_ydb:
    _log_error("not enough memory for ymldb node\n");
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
    int flushed;
    int print_level = 0;
    if(!cb || !parent || !key)
        return;

    struct ymldb *ydb = NULL;
    if (parent->type != YMLDB_BRANCH)
    {
        _log_error("\n"
            "\tUnable to delete a node from value.\n"
            "\tparent ymldb: %s, child ymldb %s\n", parent->key, key);
        return;
    }

    ydb = cp_avltree_get(parent->children, key);
    if (ydb)
    { 
        if(ydb->level <= 1) {
            _log_error("Unable to delete major ymldb branch.\n");
            cb->out.res = -1;
            return;
        }
    }
    else {
        _log_error("'%s' doesn't exists\n", key);
        cb->out.res = -1;
        return;
    }

    _log_debug("ydb key=%s\n", ydb->key);

    if (cb->last_notify)
        print_level = _ymldb_print_level(cb->last_notify, ydb);
    if (cb->reply.stream)
        ymldb_dump(cb->reply.stream, ydb, print_level, 1);
    flushed = _ymldb_reply_flush(cb, 0);
    if (flushed)
    {
        cb->last_notify = NULL;
        _ymldb_reply_init(cb);
    }
    else
        cb->last_notify = parent; // parent should be saved because of the ydb will be removed.
    ydb = cp_avltree_delete(parent->children, key);
    _ymldb_node_free(ydb);
    return;
}

void _ymldb_node_get(struct ymldb_cb *cb, struct ymldb *parent, char *key)
{
    int flushed;
    int print_level = 0;
    struct ymldb *ydb = NULL;
    if(!cb || !parent || !key)
        return;
    
    if (parent->type != YMLDB_BRANCH)
    {
        _log_error("\n"
            "\tUnable to get a node from value.\n"
            "\tparent ymldb: %s, child ymldb %s\n", parent->key, key);
        return;
    }
    ydb = cp_avltree_get(parent->children, key);
    if (!ydb)
    {
        _log_error("'%s' doesn't exists\n", key);
        cb->out.res = -1;
        return;
    }

    if (cb->last_notify)
        print_level = _ymldb_print_level(cb->last_notify, ydb);
    if (cb->reply.stream)
        ymldb_dump(cb->reply.stream, ydb, print_level, 0);
    flushed = _ymldb_reply_flush(cb, 0);
    if (flushed)
    {
        cb->last_notify = NULL;
        _ymldb_reply_init(cb);
    }
    else
        cb->last_notify = ydb;
    return;
}

int _ymldb_merge(struct ymldb_cb *cb, struct ymldb *p_ydb, int index, int p_index)
{
    yaml_node_t *node = NULL;
    if (!p_ydb)
    {
        _log_error("merge failed - unknown ydb\n");
        cb->out.res = -1;
        return -1;
    }
    node = yaml_document_get_node(cb->document, index);
    if (!node)
        return 0;

    if (p_ydb->level == 1)
    {
        if (strcmp(cb->key, p_ydb->key) != 0)
        {
            _log_error("merge failed due to key mismatch (%s, %s)\n", cb->key, p_ydb->key);
            cb->out.res = -1;
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
    {
        _log_error("delete failed - unknown ydb\n");
        cb->out.res = -1;
        return -1;
    }
    node = yaml_document_get_node(cb->document, index);
    if (!node)
        return 0;

    if (p_ydb->level == 1)
    {
        if (strcmp(cb->key, p_ydb->key) != 0)
        {
            _log_error("delete failed due to key mismatch (%s, %s)\n", cb->key, p_ydb->key);
            cb->out.res = -1;
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
            char *value = (char *)value_node->data.scalar.value;
            // _log_debug("key %s\n", key);
            if (value_node->type == YAML_SCALAR_NODE)
            {
                if(value[0] > 0) {
                    struct ymldb *ydb = NULL;
                    ydb = cp_avltree_get(p_ydb->children, key);
                    _ymldb_node_delete(cb, ydb, value);
                }
                else
                    _ymldb_node_delete(cb, p_ydb, key);
            }
            else
            { // not leaf
                struct ymldb *ydb = NULL;
                ydb = cp_avltree_get(p_ydb->children, key);
                _ymldb_delete(cb, ydb, pair->value, index);
            }
        }
    }
    break;
    case YAML_SCALAR_NODE:
    { // It is only used for single key inserted..
         char *key;
         key = (char *)node->data.scalar.value;
         _log_debug("scalar key %s, value -\n", key);
         _ymldb_node_delete(cb, p_ydb, key);
    }
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
    {
        _log_error("get failed - unknown ydb\n");
        cb->out.res = -1;
        return -1;
    }
    node = yaml_document_get_node(cb->document, index);
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
            char *value = (char *)value_node->data.scalar.value;
            // _log_debug("key %s\n", key);
            if (value_node->type == YAML_SCALAR_NODE)
            {
                if(value[0] > 0) {
                    struct ymldb *ydb = NULL;
                    ydb = cp_avltree_get(p_ydb->children, key);
                    _ymldb_node_get(cb, ydb, value);
                }
                else
                    _ymldb_node_get(cb, p_ydb, key);
            }
            else
            { // not leaf
                struct ymldb *ydb = NULL;
                ydb = cp_avltree_get(p_ydb->children, key);
                _ymldb_get(cb, ydb, pair->value, index);
            }
        }
    }
    break;
    case YAML_SCALAR_NODE:
    { // It is only used for single key inserted..
         char *key;
         key = (char *)node->data.scalar.value;
         _ymldb_node_get(cb, p_ydb, key);
    }
    break;
    case YAML_NO_NODE:
    default:
        break;
    }
    return 0;
}

int _ymldb_op_extract(struct ymldb_cb *cb, unsigned int *sequence)
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
            if (strcmp(op, YMLDB_TAG_OP_SEQ) == 0)
            {
                opcode = opcode | YMLDB_OP_SEQ;
                sscanf((char *)tag->prefix, YMLDB_TAG_SEQ"%u", sequence);
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
    return opcode;
}

static int _ymldb_state_machine(struct ymldb_cb *cb, unsigned int in_opcode, unsigned int in_sequence)
{
    int ignore = 0;
    cb->opcode = 0;
    cb->reply.no_reply = 1;
    if (!(in_opcode & YMLDB_OP_ACTION))
    {
        // do nothing if no action.
        ignore = 1;
    }
    if (cb->flags & YMLDB_FLAG_PUBLISHER) 
    {
        if (in_opcode & YMLDB_OP_SUBSCRIBER)
        {
            if(in_opcode & (YMLDB_OP_MERGE | YMLDB_OP_DELETE))
                ignore = 1;
            else // YMLDB_OP_GET, YMLDB_OP_SYNC
            {
                cb->opcode = YMLDB_OP_PUBLISHER;
                cb->opcode |= (in_opcode & (YMLDB_OP_GET | YMLDB_OP_SYNC));
                cb->reply.no_reply = 0;
            }
        }
        else if (in_opcode & YMLDB_OP_PUBLISHER)
            ignore = 1;
        else {
            cb->opcode = YMLDB_OP_PUBLISHER;
            cb->opcode |= (in_opcode & YMLDB_OP_ACTION);
            cb->reply.no_reply = 0;
            if(in_opcode & YMLDB_OP_GET)
                cb->reply.no_reply = 1;
        }
    }
    else if (cb->flags & YMLDB_FLAG_SUBSCRIBER)
    {
        if (in_opcode & YMLDB_OP_SUBSCRIBER)
            ignore = 1;
        else  if (in_opcode & YMLDB_OP_PUBLISHER)
        {
            if(in_opcode & YMLDB_OP_GET)
                ignore = 1;
            else // YMLDB_OP_MERGE, YMLDB_OP_DELETE, YMLDB_OP_SYNC
            {
                cb->opcode = YMLDB_OP_SUBSCRIBER;
                cb->opcode |= (in_opcode & (YMLDB_OP_MERGE | YMLDB_OP_DELETE | YMLDB_OP_SYNC));
            }
        }
        else {
            if(in_opcode & (YMLDB_OP_MERGE | YMLDB_OP_DELETE))
                ignore = 1;
            else // YMLDB_OP_GET, YMLDB_OP_SYNC
            {
                cb->opcode = YMLDB_OP_SUBSCRIBER;
                cb->opcode |= (in_opcode & (YMLDB_OP_GET | YMLDB_OP_SYNC));
                if(in_opcode & YMLDB_OP_SYNC)
                    cb->reply.no_reply = 0;
            }
        }
    }
    else
    { // ymldb for local user
        if (in_opcode & YMLDB_OP_SUBSCRIBER)
            ignore = 1;
        else  if (in_opcode & YMLDB_OP_PUBLISHER)
            ignore = 1;
        else
        {
            cb->opcode |= (in_opcode & YMLDB_OP_ACTION);
            cb->reply.no_reply = 1;
            if(in_opcode & YMLDB_OP_SYNC)
                ignore = 1;
        }
    }
    cb->opcode |= YMLDB_OP_SEQ;
    if(in_opcode & YMLDB_OP_SEQ) {
        cb->sequence = in_sequence;
    }
    else {
        cb->sequence = gSequence;
        gSequence++;
    }
    return ignore;
}

int _ymldb_run(struct ymldb_cb *cb, FILE *instream, FILE *outstream)
{
    int done = 0;
    yaml_parser_t *parser;

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

    parser = _yaml_parser_init(instream);
    if (!parser)
    {
        _log_error("parser init failed\n");
        return -1;
    }

    cb->out.stream = outstream;

    _log_debug("CB[%s]\n", cb->key);
    _log_debug(">>> START\n");
    while (!done)
    {
        unsigned int in_opcode = 0;
        unsigned int in_sequence = 0;
        yaml_node_t *yroot = NULL;
        /* Get the next ymldb document. */
        cb->document = _yaml_document_load(parser);
        if (!cb->document)
        {
            // _log_error("load a document - failed\n");
            cb->out.res = -1;
            break;
        }

        in_opcode = _ymldb_op_extract(cb, &in_sequence);
        yroot = yaml_document_get_root_node(cb->document);
        if (yroot)
        {
            _log_debug("IN %uth\n", in_sequence);
            _log_debug("IN %s\n", _ymldb_opcode_str(in_opcode));
            if (_ymldb_state_machine(cb, in_opcode, in_sequence))
            {
                _log_debug("result: recv %uth %s\n", in_sequence, "IGNORED");
                goto skip_document;
            }
            _log_debug("OUT %dth\n", cb->sequence);
            _log_debug("OUT %s\n", _ymldb_opcode_str(cb->opcode));

            _ymldb_reply_init(cb);
            if (in_opcode & YMLDB_OP_MERGE)
                _ymldb_merge(cb, gYdb, 1, 1);
            if (in_opcode & YMLDB_OP_DELETE)
                _ymldb_delete(cb, gYdb, 1, 1);
            if (in_opcode & YMLDB_OP_GET)
                _ymldb_get(cb, gYdb, 1, 1);
            if (in_opcode & YMLDB_OP_SYNC) {
                if (in_opcode & YMLDB_OP_PUBLISHER)
                    _ymldb_merge(cb, gYdb, 1, 1);
                else
                    _ymldb_get(cb, gYdb, 1, 1);
            }
            _ymldb_reply_flush(cb, 1); // forced flush!
            cb->last_notify = NULL;
            _log_debug("result: %s\n", cb->out.res<0?"FAILED":"OK");
        }
        else
        {
            done = 1;
        }
    skip_document:
        _yaml_document_free(cb->document);
        cb->document = NULL;
    }
    _log_debug("<<< END\n");
    
    cb->out.stream = NULL;

    _ymldb_reply_deinit(cb);
    _yaml_parser_free(parser);
    return cb->out.res;
}

int _ymldb_run_with_string(struct ymldb_cb *cb, char *ymldata, size_t ymldata_len, FILE *outstream)
{
    FILE *instream;
    if (!cb || !ymldata)
    {
        _log_error("no cb or ymldata\n");
        return -1;
    }
    if (ymldata_len <= 0)
    {
        _log_error("invalid ymldata_len %lu\n", ymldata_len);
        return -1;
    }
    _log_debug("ymldata_len %lu\n", ymldata_len);
    _log_debug("ymldata %s\n", ymldata);
    instream = fmemopen(ymldata, ymldata_len, "r");
    if (instream)
    {
        int res = _ymldb_run(cb, instream, outstream);
        fclose(instream);
        return res;
    }
    else
    {
        _log_error("fail to open instream from ymldata\n");
        return -1;
    }
}

int ymldb_run(struct ymldb_cb *cb, int infd, int outfd)
{
    int res;
    FILE *instream;
    FILE *outstream;
    if(infd < 0 ) {
        _log_error("invalid infd.\n");
        return -1;
    }
    _log_debug("\n");
    if(outfd <= 0) 
        outstream = NULL;
    else
        outstream = _ymldb_fopen_from_fd(outfd, "w");
    
    instream = _ymldb_fopen_from_fd(infd, "r");
    res = _ymldb_run(cb, instream, outstream);
    fclose(instream);
    return res;
}

struct ymldb_cb *ymldb_create(char *key, unsigned int flags)
{
    struct ymldb_cb *cb;
    if (!key)
    {
        _log_error("no key\n");
    }

    // init top
    if (!gYdb)
    {
        gYdb = _ymldb_node_merge(NULL, NULL, YMLDB_BRANCH, "top", NULL);
        if (!gYdb)
        {
            _log_error("gYdb failed.\n");
            return NULL;
        }
    }

    if (!gYcb)
    {
        gYcb = cp_avltree_create((cp_compare_fn)strcmp);
        if (!gYcb)
        {
            _log_error("gYcb failed.\n");
            return NULL;
        }
    }

    if (cp_avltree_get(gYdb->children, key))
    {
        _log_error("key exsits.\n");
        return NULL;
    }

    cb = malloc(sizeof(struct ymldb_cb));
    if (!cb)
    {
        _log_error("alloc failed.\n");
        return NULL;
    }
    memset(cb, 0x0, sizeof(struct ymldb_cb));
    cb->key = strdup(key);
    if (!cb->key)
    {
        _log_error("key alloc failed.\n");
        free(cb);
        return NULL;
    }

    cb->document = NULL;
    cb->flags = 0;
    cb->reply.stream = NULL;
    cb->last_notify = NULL;
    cb->reply.no_reply = 0;
    cb->sequence = 0;
    
    cb->fd_publisher = 0;
    memset(cb->fd_subscriber, 0x0, sizeof(cb->fd_subscriber));
    cb->ydb = _ymldb_node_merge(cb, gYdb, YMLDB_BRANCH, key, NULL);
    if (!cb->ydb)
    {
        _log_error("init failed.\n");
        free(cb->key);
        free(cb);
        return NULL;
    }
    cb->last_notify = NULL;

    if (flags & YMLDB_FLAG_PUBLISHER || flags & YMLDB_FLAG_SUBSCRIBER)
        ymldb_conn_init(cb, flags);
    // if (flags & YMLDB_FLAG_LOCAL)
    //     ymldb_local_init(cb, option); // option is fd if YMLDB_FLAG_LOCAL.
    cp_avltree_insert(gYcb, cb->key, cb);
    return cb;
}

int ymldb_conn_deinit(struct ymldb_cb *cb)
{
    if (!cb)
    {
        _log_error("no cb\n");
        return -1;
    }

    _log_debug("deinit conn\n");

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
        _log_error("no cb\n");
        return -1;
    }
    _log_debug("init conn\n");
    if (cb->flags & YMLDB_FLAG_CONN)
    {
        ymldb_conn_deinit(cb);
    }
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        _log_error("socket failed (%s)\n", strerror(errno));
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
        // cb->flags |= (flags & YMLDB_FLAG_SYNC)?YMLDB_FLAG_SYNC:0;
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

int ymldb_conn_set(struct ymldb_cb *cb, fd_set *set)
{
    int max = 0;
    if (!cb || !set)
    {
        _log_error("no cb or set\n");
        return 0;
    }
    _log_debug("set conn (FD_SET)\n");
    if (cb->flags & YMLDB_FLAG_CONN)
    {
        if (cb->flags & YMLDB_FLAG_RECONNECT)
        {
            _log_debug("skipped due to RECONN\n");
            return max;
        }
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
    _log_debug("maximum fd: %d\n", max);
    return max;
}

int ymldb_conn_recv(struct ymldb_cb *cb, fd_set *set)
{
    if (!cb || !set)
    {
        _log_error("no cb or set\n");
        return -1;
    }
    _log_debug("recv conn\n");
    if (!(cb->flags & YMLDB_FLAG_CONN))
    {
        _log_error("no required conn.\n");
        return -1;
    }
    if (cb->flags & YMLDB_FLAG_RECONNECT)
    {
        _log_debug("reinit conn\n");
        return ymldb_conn_init(cb, cb->flags);
    }
    if (cb->fd_publisher)
    {
        if (FD_ISSET(cb->fd_publisher, set))
        {
            if (cb->flags & YMLDB_FLAG_PUBLISHER)
            { // PUBLISHER
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
                        ymldb_sync(cb, cb->key);
                        break;
                    }
                }
                FD_CLR(cb->fd_publisher, set);
                if (i >= YMLDB_SUBSCRIBER_MAX)
                {
                    _log_error("subscription over..\n");
                }

            }
            else if (cb->flags & YMLDB_FLAG_SUBSCRIBER)
            { // SUBSCRIBER
                int inlen;
                char *inbuf = malloc(YMLDB_STREAM_BUF_SIZE);
                if (!inbuf)
                {
                    _log_error("inbuf alloc failed\n");
                    return -1;
                }
                inlen = read(cb->fd_publisher, inbuf, YMLDB_STREAM_BUF_SIZE);
                if (inlen <= 0)
                {
                    cb->flags |= YMLDB_FLAG_RECONNECT;
                    if(inlen < 0)
                        _log_error("conn (%d) read failed (%s)\n", cb->fd_publisher,  strerror(errno));
                    else
                        _log_error("conn (%d) closed - EOF\n", cb->fd_publisher);
                    _log_debug("retry to connect to publisher.\n");
                    free(inbuf);
                    return -1;
                }
                inbuf[inlen] = 0;
                _log_debug("inlen=%d inbuf=%s\n", inlen, inbuf);
                FD_CLR(cb->fd_publisher, set);
                _ymldb_run_with_string(cb, inbuf, inlen, NULL);
                free(inbuf);
            }
        }
    }
    if (cb->flags & YMLDB_FLAG_PUBLISHER)
    {
        for (int i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
        {
            if (FD_ISSET(cb->fd_subscriber[i], set))
            {
                int inlen;
                char *inbuf = malloc(YMLDB_STREAM_BUF_SIZE);
                if (!inbuf)
                {
                    _log_error("inbuf alloc failed\n");
                    continue;
                }
                inlen = read(cb->fd_subscriber[i], inbuf, YMLDB_STREAM_BUF_SIZE);
                if (inlen <= 0)
                {
                    if(inlen < 0)
                        _log_error("read failed (%s)\n", strerror(errno));
                    else
                        _log_error("conn (%d) closed - EOF\n", cb->fd_subscriber[i]);
                    _log_debug("close the conn (%d).\n", cb->fd_subscriber[i]);
                    FD_CLR(cb->fd_subscriber[i], set);
                    close(cb->fd_subscriber[i]);
                    cb->fd_subscriber[i] = 0;
                    free(inbuf);
                    continue;
                }

                inbuf[inlen] = 0;
                FD_CLR(cb->fd_subscriber[i], set);
                _ymldb_run_with_string(cb, inbuf, inlen, NULL);
                free(inbuf);
            }
        }
    }
    return 0;
}

static int _ymldb_conn_send(struct ymldb_cb *cb)
{
    int res = 0;
    int sent = 0;
    int retry = 0;
    if (cb->reply.no_reply)
    {
        _log_debug("no reply\n");
        return 0;
    }
    if (cb->flags & YMLDB_FLAG_RECONNECT)
    {
        _log_error("flags: need to reconnect to '%s'.\n", cb->key);
        return -1;
    }
    if (cb->fd_publisher <= 0)
    {
        cb->flags |= YMLDB_FLAG_RECONNECT;
        _log_error("fd_publisher: no fd_publisher for '%s'\n", cb->key);
        return -1;
    }
    if (cb->flags & YMLDB_FLAG_SUBSCRIBER)
    {
    subscriber_rewrite:
        res = write(cb->fd_publisher, cb->reply.buf + sent, ftell(cb->reply.stream) - sent);
        if (res < 0)
        {
            cb->flags |= YMLDB_FLAG_RECONNECT;
            _log_error("fd_publisher: fail to send fd_publisher(%d) (%s)\n",
                       cb->fd_publisher, strerror(errno));
            return -1;
        }
        sent = res + sent;
        if (sent < ftell(cb->reply.stream) && retry < 3)
        {
            retry++;
            goto subscriber_rewrite;
        }
    }
    else if (cb->flags & YMLDB_FLAG_PUBLISHER)
    {
        // if(cb->flags & YMLDB_FLAG_SYNC) {
        //     // [FIXME] block to send change notification.
        //     return 0;
        // }
        for (int i = 0; i < YMLDB_SUBSCRIBER_MAX; i++)
        {
            if (cb->fd_subscriber[i])
            {
                sent = 0;
                retry = 0;
            publisher_rewrite:
                res = write(cb->fd_subscriber[i], cb->reply.buf + sent, ftell(cb->reply.stream) - sent);
                if (res < 0)
                {
                    _log_error("fd_subscriber: close fd_subscriber(%d) due to (%s)\n",
                               cb->fd_subscriber[i], strerror(errno));
                    close(cb->fd_subscriber[i]);
                    cb->fd_subscriber[i] = 0;
                    continue;
                }
                sent = res + sent;
                if (sent < ftell(cb->reply.stream) && retry < 3)
                {
                    retry++;
                    goto publisher_rewrite;
                }
            }
        }
    }
    return 0;
}

void ymldb_destroy(struct ymldb_cb *cb)
{
    if (!cb)
        return;

    cp_avltree_delete(gYcb, cb->key);

    if (cb->ydb)
    {
        struct ymldb *ydb = cb->ydb;
        if (ydb->parent)
        {
            cp_avltree_delete(cb->ydb->parent->children, ydb->key);
        }
        _ymldb_node_free(ydb);
    }

    if (cb->document)
        _yaml_document_free(cb->document);
    _ymldb_reply_deinit(cb);
    ymldb_conn_deinit(cb);
    if (cb->key)
        free(cb->key);
    if (cb->reply.stream)
        fclose(cb->reply.stream);
    free(cb);
    if (cp_avltree_count(gYcb) <= 0)
    {
        _ymldb_node_free(gYdb);
        cp_avltree_destroy(gYcb);
        _log_debug("all destroyed ...\n");
        gYcb = NULL;
        gYdb = NULL;
    }
}

struct ymldb_stream *ymldb_stream_alloc(size_t len, char *rw)
{
    struct ymldb_stream *buf;
    if(len <= 0) {
        return NULL;
    }
    buf = malloc(len+sizeof(FILE *)+sizeof(size_t)+4);
    if(buf) {
        buf->stream = fmemopen(buf->buf, len, rw);
        buf->buflen = len;
        if(!buf->stream) {
            free(buf);
            return NULL;
        }
    }
    return buf;
}

void ymldb_stream_free(struct ymldb_stream *buf)
{
    if(buf) {
        if(buf->stream)
            fclose(buf->stream);
        free(buf);
    }
}

int _ymldb_push(struct ymldb_cb *cb, FILE *outstream, unsigned int opcode, char *format, ...)
{
    int res;
    struct ymldb_stream *input;
    if (!cb || opcode == 0)
    {
        _log_error("no cb or opcode\n");
        return -1;
    }

    _log_debug("\n");
    input = ymldb_stream_alloc(512, "w+");
    if(!input) {
        _log_error("fail to open ymldb stream\n");
        return -1;
    }
    // write ymldb data to the streambuf
    ymldb_dump_start(input->stream, opcode, 0);
    va_list args;
    va_start(args, format);
    vfprintf(input->stream, format, args);
    va_end(args);
    ymldb_dump_end(input->stream);
    fflush(input->stream);
    fseek(input->stream, 0, SEEK_SET);

    res = _ymldb_run(cb, input->stream, outstream);
    _log_debug("result: %s\n", res<0?"FAILED":"OK");
    ymldb_stream_free(input);
    return res;
}

int ymldb_push(struct ymldb_cb *cb, char *format, ...)
{
    int res;
    struct ymldb_stream *input;
    unsigned int opcode = YMLDB_OP_MERGE; 
    if (!cb)
    {
        _log_error("no cb\n");
        return -1;
    }
    _log_debug("\n");
    input = ymldb_stream_alloc(512, "w+");
    if(!input) {
        _log_error("fail to open ymldb stream\n");
        return -1;
    }
    // write ymldb data to the streambuf
    ymldb_dump_start(input->stream, opcode, 0);
    va_list args;
    va_start(args, format);
    vfprintf(input->stream, format, args);
    va_end(args);
    ymldb_dump_end(input->stream);
    fflush(input->stream);
    fseek(input->stream, 0, SEEK_SET);

    res = _ymldb_run(cb, input->stream, NULL);
    _log_debug("result: %s\n", res<0?"FAILED":"OK");
    ymldb_stream_free(input);
    return res;
}


void ymldb_remove_specifiers(FILE *dest, char *src)
{
    static char *specifiers_of_fscanf = "iudoxfegacsp";
    int specifier_pos = -1;
    for(int i=0; src[i]!=0; i++)
    {
        if(specifier_pos > -1) {
            // _log_debug("i=%d, %c\n", i, src[i]);
            if(src[i] == ' ' || src[i] == '\n' || src[i] == '\t') {
                // _log_debug("i=%d, %c\n", i, src[i]);
                specifier_pos = -1;
                fputc(src[i], dest);
            }
            else if (strchr(specifiers_of_fscanf, src[i])) {
                // _log_debug("i=%d, %c\n", i, src[i]);
                specifier_pos = -1;
            }
            continue;
        }
        else if(src[i] == '%') {
            // _log_debug("i=%d, %c\n", i, src[i]);
            if(src[i+1]=='%') {
                // _log_debug("i=%d, %c\n", i, src[i]);
                fputc(src[i], dest);
                fputc(src[i+1], dest);
                i++;
                continue;
            }
            specifier_pos = i;
            continue;
        }
        else if(src[i] == '-') { // "- ""
            if(src[i+1] == ' ') { // leaflist ymldb.
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
int ymldb_pull(struct ymldb_cb *cb, char *format, ...)
{
    int res;
    int opcode = YMLDB_OP_GET;
    struct ymldb_stream *input;
    struct ymldb_stream *output;
    if (!cb)
    {
        _log_error("no cb\n");
        return -1;
    }
    _log_debug("\n");
    input = ymldb_stream_alloc(512, "w+");
    if(!input) {
        _log_error("fail to open ymldb stream\n");
        return -1;
    }
    output = ymldb_stream_alloc(512, "w+");
    if(!output) {
        _log_error("fail to open ymldb stream\n");
        ymldb_stream_free(input);
        return -1;
    }
    ymldb_dump_start(input->stream, opcode, 0);
    ymldb_remove_specifiers(input->stream, format);
    ymldb_dump_end(input->stream);
    fflush(input->stream);
    fseek(input->stream, 0, SEEK_SET);
    res = _ymldb_run(cb, input->stream, output->stream);
    _log_debug("result: %s\n", res<0?"FAILED":"OK");
    printf("result: %s\n", output->buf);
    if (res >= 0)
    { // success
        // fflush(output->stream);
        char *doc_body = strstr(output->buf, "---");
        if(doc_body)
            doc_body = doc_body + 4;
        else
            doc_body = output->buf;

        va_list args;
        va_start(args, format);
        vsscanf(doc_body, format, args);
        va_end(args);
    }
    ymldb_stream_free(input);
    ymldb_stream_free(output);
    return 0;
}

// write a key and value
int _ymldb_write(struct ymldb_cb *cb, FILE *outstream, unsigned int opcode, ...)
{
    int res;
    int level = 0;
    struct ymldb_stream *input;
    if (!cb || opcode == 0)
    {
        _log_error("no cb or opcode\n");
        return -1;
    }

    _log_debug("CB[%s]\n", cb->key);
    input = ymldb_stream_alloc(256, "w+");
    if(!input) {
        _log_error("fail to open ymldb stream\n");
        return -1;
    }

    ymldb_dump_start(input->stream, opcode, 0);
    va_list args;
    va_start(args, opcode);
    char *cur_token;
    char *next_token;
    cur_token = va_arg(args, char *);
    while(cur_token!=NULL) {
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
    ymldb_dump_end(input->stream);
    fflush(input->stream);
    fseek(input->stream, 0, SEEK_SET);
    // printf("input->buf %s\n", input->buf);
    res = _ymldb_run(cb, input->stream, outstream);
    _log_debug("result: %s\n", res<0?"FAILED":"OK");
    ymldb_stream_free(input);
    return res;
}

// read a value by a key.
char *_ymldb_read(struct ymldb_cb *cb, ...)
{ // directly access to ymldb.
    struct ymldb *ydb;
    if (!cb)
    {
        _log_error("no cb\n");
        return NULL;
    }
    if(!gYdb) {
        _log_error("no created ydb.\n");
        return NULL;
    }
    ydb = gYdb;
    va_list args;
    va_start(args, cb);
    char *cur_token;
    char *next_token;
    cur_token = va_arg(args, char *);
    while(cur_token!=NULL) {
        next_token = va_arg(args, char *);
        if (next_token)
        {
            if(ydb) {
                if(ydb->type == YMLDB_BRANCH) {
                    ydb = cp_avltree_get(ydb->children, cur_token);
                }
                else { // not exist..
                    ydb = NULL;
                    break;
                }
            }
            else {
                // not exist..
                break;
            }
        }
        else {
            if(ydb) {
                if(ydb->type == YMLDB_BRANCH) {
                    ydb = cp_avltree_get(ydb->children, cur_token);
                }
            }
        }
        cur_token = next_token;
    };
    va_end(args);
    if(ydb) {
        if(ydb->type == YMLDB_BRANCH)
            return NULL;
        return ydb->value;
    }
    else
        return NULL;
}

void ymldb_dump_all(FILE *stream)
{
    ymldb_dump(stream, gYdb, 0, 0);
    fprintf(stream, "\n  @@ alloc_cnt %d @@\n", alloc_cnt);
}