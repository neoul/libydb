#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <yaml.h>
#include <cprops/avl.h>
#include <cprops/linked_list.h>
#include "ymldb.h"

// for debug - start
#define _log_debug(...)                                  \
    do                                                   \
    {                                                    \
        fprintf(stdout, "____%.*s: ", 16, __FUNCTION__); \
        fprintf(stdout, __VA_ARGS__);                    \
    } while (0)

#define _log_error(...)                                  \
    do                                                   \
    {                                                    \
        fprintf(stdout, "____%.*s: ", 16, __FUNCTION__); \
        fprintf(stderr, __VA_ARGS__);                    \
    } while (0)

#define _out(FP, ...)                 \
    do                                \
    {                                 \
        if (FP)                       \
        {                             \
            fprintf(FP, __VA_ARGS__); \
        }                             \
    } while(0)

int _log_error_ydb_parser(yaml_parser_t *parser)
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

int _log_error_ydb_emitter(yaml_emitter_t *emitter)
{
    switch (emitter->error)
    {
    case YAML_MEMORY_ERROR:
        _log_error("ymldb:emitter:no memory\n");
        break;

    case YAML_WRITER_ERROR:
        _log_error("ymldb:emitter:write error:%s\n", emitter->problem);
        break;

    case YAML_EMITTER_ERROR:
        _log_error("ymldb:emitter:emitter error: %s\n", emitter->problem);
        break;

    default:
        /* Couldn't happen. */
        _log_error("ymldb:emitter:internal error\n");
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

static struct ymldb *gTop;

FILE *fopen_from_fd(int fd, char *rw)
{
    int dup_fd = dup(fd);
    return fdopen(dup_fd, rw);
}


char *_str_dump(const char *src)
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

char *_yaml_type_dump(yaml_node_type_t t)
{
    char *typestr[] = {
        "type:empty",
        "type:scalar",
        "type:sequence",
        "type:mapping"};
    if (YAML_MAPPING_NODE < t)
        return "type:unknown";
    return typestr[t];
}

// yaml func
void yaml_document_free(yaml_document_t *document)
{
    if (!document)
        return;
    if (document->nodes.start)
    {
        yaml_document_delete(document);
    }
    free(document);
}

void yaml_parser_free(yaml_parser_t *parser)
{
    if (!parser)
        return;
    if (parser->raw_buffer.start || parser->error)
    {
        yaml_parser_delete(parser);
    }
    free(parser);
}

void yaml_emitter_free(yaml_emitter_t *emitter)
{
    if (!emitter)
        return;
    if (emitter->buffer.start || emitter->error)
    {
        yaml_emitter_delete(emitter);
    }
    free(emitter);
}

void yaml_emitter_flush_and_free(yaml_emitter_t *emitter, yaml_document_t *document)
{
    if (!emitter || !document)
    {
        yaml_document_free(document);
        yaml_emitter_free(emitter);
        return;
    }

    if (!yaml_emitter_dump(emitter, document))
    {
        _log_error_ydb_emitter(emitter);
    }
    yaml_document_free(document);
    yaml_emitter_free(emitter);
    return;
}

yaml_document_t *yaml_document_init(char *op)
{
    yaml_document_t *document;
    yaml_tag_directive_t tag[2];

    document = malloc(sizeof(yaml_document_t));
    if (!document)
    {
        _log_error("ymldb:document: memory alloc failed.\n");
        return NULL;
    }
    // create tag_directives
    if (strcmp(op, YMLDB_TAG_OP_DELETE) == 0)
    {
        tag[0].handle = (yaml_char_t *)YMLDB_TAG_OP_DELETE;
        tag[0].prefix = (yaml_char_t *)YMLDB_TAG_DELETE;
    }
    else if (strcmp(op, YMLDB_TAG_OP_GET) == 0)
    {
        tag[0].handle = (yaml_char_t *)YMLDB_TAG_OP_GET;
        tag[0].prefix = (yaml_char_t *)YMLDB_TAG_GET;
    }
    else
    {
        tag[0].handle = (yaml_char_t *)YMLDB_TAG_OP_MERGE;
        tag[0].prefix = (yaml_char_t *)YMLDB_TAG_MERGE;
    }
    tag[1].handle = NULL;
    tag[1].prefix = NULL;
    if (!yaml_document_initialize(document, NULL, &tag[0], &tag[1], 0, 0))
    {
        _log_error("ymldb:document: init failed\n");
        yaml_document_free(document);
        return NULL;
    }
    return document;
}

yaml_document_t *yaml_document_load(yaml_parser_t *parser)
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
        _log_error_ydb_parser(parser);
        yaml_document_free(document);
        // yaml_parser_free(parser);
        return NULL;
    }
    return document;
}

yaml_parser_t *yaml_parser_init(FILE *in)
{
    yaml_parser_t *parser;
    if (!in)
    {
        _log_error("ymldb:parser: invalid in.\n");
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
        _log_error_ydb_parser(parser);
        yaml_parser_free(parser);
        return NULL;
    }
    yaml_parser_set_input_file(parser, in);
    return parser;
}

yaml_emitter_t *yaml_emitter_init(FILE *out)
{
    yaml_emitter_t *emitter;
    if (!out)
    {
        return NULL;
    }
    emitter = malloc(sizeof(yaml_emitter_t));
    if (!emitter)
    {
        _log_error("ymldb:emitter: memory alloc failed.\n");
        return NULL;
    }
    if (!yaml_emitter_initialize(emitter))
    {
        _log_error_ydb_emitter(emitter);
        yaml_emitter_free(emitter);
        return NULL;
    }
    yaml_emitter_set_output_file(emitter, out);
    yaml_emitter_set_canonical(emitter, 0);
    yaml_emitter_set_unicode(emitter, 0);
    return emitter;
}

cp_list *ymldb_traverse_ancestors(struct ymldb *ydb, int traverse_level)
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

void ymldb_traverse_free(cp_list *templist)
{
    if (templist)
    {
        cp_list_destroy(templist);
    }
}

struct ymldb *ymldb_get_ancestor(struct ymldb *ydb, int level)
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

static int ymldb_node_dump(void *n, void *dummy)
{
    cp_avlnode *node = n;
    struct ymldb *ydb = node->value;
    struct ymldb_cb *cb = dummy;
    ymldb_dump(cb, ydb, ydb->level, 0); // not print parents
    return 0;
}

void ymldb_dump(struct ymldb_cb *cb, struct ymldb *ydb, int print_level, int no_print_children)
{
    cp_list *ancestors;
    if (!cb || !ydb)
        return;
    if (print_level < ydb->level)
    { // print parents
        struct ymldb *ancestor;
        cp_list_iterator iter;
        ancestors = ymldb_traverse_ancestors(ydb, print_level);
        cp_list_iterator_init(&iter, ancestors, COLLECTION_LOCK_NONE);
        while ((ancestor = cp_list_iterator_next(&iter)))
        {
            if (ancestor->level == 0)
                continue;
            switch (ancestor->type)
            {
            case YMLDB_BRANCH:
                _log_debug("%.*s%s:\n", (ancestor->level - 1) * 2, gSpace, ancestor->key);
                _out(cb->out, "%.*s%s:\n", (ancestor->level - 1) * 2, gSpace, ancestor->key);
                break;
            case YMLDB_LEAFLIST:
                _log_debug("%.*s- %s\n", (ancestor->level - 1) * 2, gSpace, ancestor->key);
                _out(cb->out, "%.*s- %s\n", (ancestor->level - 1) * 2, gSpace, ancestor->key);
                break;
            case YMLDB_LEAF:
                _log_debug("%.*s%s: %s\n", (ancestor->level - 1) * 2, gSpace, ancestor->key, ancestor->value);
                _out(cb->out, "%.*s%s: %s\n", (ancestor->level - 1) * 2, gSpace, ancestor->key, ancestor->value);
                break;
            }
        }
        ymldb_traverse_free(ancestors);
    }

    if (ydb->type == YMLDB_BRANCH)
    {
        if (ydb->level != 0)
        { // not print out for top node
            _log_debug("%.*s%s:\n", (ydb->level - 1) * 2, gSpace, ydb->key);
            _out(cb->out, "%.*s%s:\n", (ydb->level - 1) * 2, gSpace, ydb->key);
        }
        if (no_print_children)
            return;
        cp_avltree_callback(ydb->children, ymldb_node_dump, cb);
    }
    else if (ydb->type == YMLDB_LEAFLIST)
    {
        _log_debug("%.*s- %s\n", (ydb->level - 1) * 2, gSpace, ydb->key);
        _out(cb->out, "%.*s- %s\n", (ydb->level - 1) * 2, gSpace, ydb->key);
    }
    else
    {
        _log_debug("%.*s%s: %s\n", (ydb->level - 1) * 2, gSpace, ydb->key, ydb->value);
        _out(cb->out, "%.*s%s: %s\n", (ydb->level - 1) * 2, gSpace, ydb->key, ydb->value);
    }
    return;
}

// Remove all subtree and data
void _ymldb_node_free(void *vdata)
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

int _print_item(void *item, void *dummy)
{
    if (dummy)
        _log_debug(" -- %s=%s\n", (char *)dummy, (char *)item);
    else
        _log_debug(" -- %s\n", (char *)item);
    return 0;
}

int _ymldb_get_print_level(struct ymldb *last_ydb, struct ymldb *cur_ydb)
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
        ancestor1 = ymldb_get_ancestor(cur_ydb, print_level);
        ancestor2 = ymldb_get_ancestor(last_ydb, print_level);
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
    int print_level = 0;
    if (!cb || !ydb)
        return;
    if (cb->last_notify)
        print_level = _ymldb_get_print_level(cb->last_notify, ydb);
    ymldb_dump(cb, ydb, print_level, 0);
    cb->last_notify = ydb;
    return;
}

void _ymldb_node_delete_notify(struct ymldb_cb *cb, struct ymldb *parent, char *key)
{
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
        print_level = _ymldb_get_print_level(cb->last_notify, ydb);
    ymldb_dump(cb, ydb, print_level, 1);
    cb->last_notify = parent; // parent should be saved because of the ydb will be removed.
    return;
}

void _ymldb_node_get_reply(struct ymldb_cb *cb, struct ymldb *parent, char *key)
{
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
        print_level = _ymldb_get_print_level(cb->last_notify, ydb);
    ymldb_dump(cb, ydb, print_level, 0);
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
    node = yaml_document_get_node(cb->in_document, index);
    if (!p_ydb)
        return -1;

    if (!node)
        return 0;

    if(p_ydb->level == 1) {
        if(strcmp(cb->key, p_ydb->key) != 0) {
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
            yaml_node_t *node = yaml_document_get_node(cb->in_document, *item);
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
            yaml_node_t *key_node = yaml_document_get_node(cb->in_document, pair->key);
            yaml_node_t *value_node = yaml_document_get_node(cb->in_document, pair->value);
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
    node = yaml_document_get_node(cb->in_document, index);
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
            yaml_node_t *node = yaml_document_get_node(cb->in_document, *item);
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
            yaml_node_t *key_node = yaml_document_get_node(cb->in_document, pair->key);
            yaml_node_t *value_node = yaml_document_get_node(cb->in_document, pair->value);
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
    node = yaml_document_get_node(cb->in_document, index);
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
            yaml_node_t *node = yaml_document_get_node(cb->in_document, *item);
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
            yaml_node_t *key_node = yaml_document_get_node(cb->in_document, pair->key);
            yaml_node_t *value_node = yaml_document_get_node(cb->in_document, pair->value);
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
    if (!cb || !cb->in_document)
        return opcode;
    if (cb->in_document->tag_directives.start != cb->in_document->tag_directives.end)
    {
        char *op;
        yaml_tag_directive_t *tag;
        for (tag = cb->in_document->tag_directives.start;
             tag != cb->in_document->tag_directives.end; tag++)
        {
            op = (char *)tag->handle;
            if (strcmp(op, YMLDB_TAG_OP_MERGE) == 0) {
                opcode = opcode | YMLDB_OP_MERGE;
            }
            else if (strcmp(op, YMLDB_TAG_OP_DELETE) == 0) {
                opcode = opcode | YMLDB_OP_DELETE;
            }
            else if (strcmp(op, YMLDB_TAG_OP_GET) == 0) {
                opcode = opcode | YMLDB_OP_GET;
            }
            else if (strcmp(op, YMLDB_TAG_OP_SUBSCRIBE) == 0) {
                opcode = opcode | YMLDB_OP_SUBSCRIBE;
            }
            else if (strcmp(op, YMLDB_TAG_OP_UNSUBSCRIBE) == 0) {
                opcode = opcode | YMLDB_OP_UNSUBSCRIBE;
            }
            else if (strcmp(op, YMLDB_TAG_OP_PUBLISH) == 0) {
                opcode = opcode | YMLDB_OP_PUBLISH;
            }
        }
    }
    cb->opcode = opcode;
    return opcode;
}

void _ymldb_dump_start(FILE *out, int opcode, int sequence)
{
    _out(out, "# %d\n", sequence);

    // %TAG !merge! actusnetworks.com:op:
    if (opcode & YMLDB_OP_MERGE) {
        _out(out, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_MERGE, YMLDB_TAG_MERGE);
    }
    if (opcode & YMLDB_OP_DELETE) {
        _out(out, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_DELETE, YMLDB_TAG_DELETE);
    }
    if (opcode & YMLDB_OP_GET) {
        _out(out, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_GET, YMLDB_TAG_GET);
    }
    if (opcode & YMLDB_OP_SUBSCRIBE) {
        _out(out, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_SUBSCRIBE, YMLDB_TAG_SUBSCRIBE);
    }
    if (opcode & YMLDB_OP_UNSUBSCRIBE) {
        _out(out, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_UNSUBSCRIBE, YMLDB_TAG_UNSUBSCRIBE);
    }
    if (opcode & YMLDB_OP_PUBLISH) {
        _out(out, "%s %s %s\n", "%TAG", YMLDB_TAG_OP_PUBLISH, YMLDB_TAG_PUBLISH);
    }
    _out(out, "---\n");
}


void _ymldb_dump_end(FILE *out)
{
    _out(out, "...\n\n");
}


int ymldb_construct(struct ymldb_cb *cb)
{
    int done = 0;
    if (!cb)
    {
        _log_error("ymldb:construct:invalid ymldb_cb\n");
        return -1;
    }
    if (!cb->in)
    {
        _log_error("ymldb:construct:invalid input stream\n");
        return -1;
    }

    cb->parser = yaml_parser_init(cb->in);
    if (!cb->parser)
    {
        _log_error("ymldb:construct:parser init failed\n");
        return -1;
    }

    _log_debug("> start\n");
    while (!done)
    {
        int opcode = 0;
        yaml_node_t *yroot = NULL;
        /* Get the next ymldb document. */
        cb->in_document = yaml_document_load(cb->parser);
        if (!cb->in_document)
        {
            _log_error("ymldb:construct:load a document from parser - failed\n");
            break;
        }

        opcode = _ymldb_op_extract(cb);
        yroot = yaml_document_get_root_node(cb->in_document);
        if (yroot)
        {
            cb->sequence++;
            _log_debug("%dth %d\n", cb->sequence, opcode);

            // cb->emitter = yaml_emitter_init(cb->out);
            // cb->out_document = yaml_document_init(opcode);

            _ymldb_dump_start(cb->out, cb->opcode, cb->sequence);
            if (opcode & YMLDB_OP_PUBLISH)
            {
            }
            if (opcode & YMLDB_OP_SUBSCRIBE)
            {
            }
            if (opcode & YMLDB_OP_UNSUBSCRIBE)
            {
            }

            if (opcode & YMLDB_OP_MERGE)
                _ymldb_merge(cb, cb->ydb, 1, 1);
            if (opcode & YMLDB_OP_DELETE)
                _ymldb_delete(cb, cb->ydb, 1, 1);
            if (opcode & YMLDB_OP_GET)
                _ymldb_get(cb, cb->ydb, 1, 1);

            _ymldb_dump_end(cb->out);
            cb->last_notify = NULL;

            // yaml_emitter_flush_and_free(cb->emitter, cb->out_document);
            // cb->out_document = NULL;
            // cb->emitter = NULL;
        }
        else
        { /* Check if this is the in end. */
            done = 1;
        }

        yaml_document_free(cb->in_document);
        cb->in_document = NULL;
        _log_debug("\n");
    }
    _log_debug("> end\n\n");
    yaml_parser_free(cb->parser);
    cb->parser = NULL;
    return 0;
}

struct ymldb_cb *ymldb_create(char *owner, char *key, FILE *in, FILE *out)
{
    struct ymldb_cb *cb;
    if(!key) {
        _log_error("ymldb:init: no key\n");
    }
    if(!owner) {
        _log_error("ymldb:init: no owner\n");
    }

    // init top
    if(!gTop) {
        gTop = _ymldb_node_merge(NULL, NULL, YMLDB_BRANCH, "top", NULL);
        if (!gTop)
        {
            _log_error("ymldb:init:top failed.\n");
            return NULL;
        }
    }
    if(cp_avltree_get(gTop->children, key)) {
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
    if(!cb->key) {
        _log_error("ymldb:init: key alloc failed.\n");
        free(cb);
        return NULL;
    }

    cb->owner = strdup(owner);
    if(!cb->owner) {
        _log_error("ymldb:init: owner alloc failed.\n");
        free(cb->key);
        free(cb);
        return NULL;
    }
    
    cb->parser = NULL;
    cb->in_document = NULL;
    cb->out_document = NULL;
    cb->emitter = NULL;
    cb->sequence = 0;
    cb->in = in;
    cb->out = out;

    cb->ydb = _ymldb_node_merge(cb, gTop, YMLDB_BRANCH, key, NULL);
    if (!cb->ydb)
    {
        _log_error("ymldb:init: init failed.\n");
        free(cb->key);
        free(cb->owner);
        free(cb);
        return NULL;
    }
    return cb;
}

struct ymldb_cb *ymldb_create_with_fd(char *owner, char *key, int infd, int outfd)
{
    FILE *in = fopen_from_fd(infd, "r");
    FILE *out = fopen_from_fd(outfd, "w");
    return ymldb_create(owner, key, in, out);
}

void ymldb_destroy(struct ymldb_cb *cb)
{
    if (!cb)
        return;
    if (cb->parser)
        yaml_parser_free(cb->parser);
    if (cb->emitter)
        yaml_emitter_free(cb->emitter);
    if (cb->in_document)
        yaml_document_free(cb->in_document);
    if (cb->out_document)
        yaml_document_free(cb->out_document);
    if (cb->ydb)
    {
        struct ymldb *ydb = cb->ydb;
        while (ydb)
        {
            if (ydb->parent)
                ydb = ydb->parent;
            else
                break;
        }
        _ymldb_node_free(ydb);
    }
    if(cb->key)
        free(cb->key);
    if(cb->owner)
        free(cb->owner);
    if(cb->in)
        fclose(cb->in);
    if(cb->out)
        fclose(cb->out);
    free(cb);
}


int _ymldb_push(struct ymldb_cb *cb, char *yml_data)
{
    _log_debug("yml_data=\n");
    _log_debug("\n'%s'\n",yml_data);
    if(cb) {
        FILE *backup = cb->in;
        cb->in = fmemopen(yml_data, strlen(yml_data), "r");
        if(cb->in) {
            ymldb_construct(cb);
            fclose(cb->in);
        }
        cb->in = backup;
    }
    return 0;
}

int ymldb_push (struct ymldb_cb *cb, int opcode, char * format, ...)
{
    FILE *stream;
    char streambuf[512] = {0,};
    if(!cb || opcode == 0) {
        _log_error("ymldb:push: no cb or opcode\n");
        return -1;
    }

    stream = fmemopen(streambuf, sizeof(streambuf), "w");
    if(!stream) {
        _log_error("ymldb:push: unable fmemopen stream\n");
        return -1;
    }
    // write ymldb data to the streambuf
    _ymldb_dump_start(stream, opcode, 0);
    va_list args;
    va_start (args, format);
    vfprintf (stream, format, args);
    va_end (args);
    _ymldb_dump_end(stream);
    fclose(stream);
    // read ymldb data from the streambuf
    return _ymldb_push(cb, streambuf);
}



int ymldb_write(struct ymldb_cb *cb, int opcode, int num, ...)
{
    int i;
    char yml_data[256] = {0,};
    char *token;
    FILE *stream;
    va_list args;

    if(!cb || opcode == 0) {
        _log_error("ymldb:write: no cb or opcode\n");
        return -1;
    }

    stream = fmemopen(yml_data, sizeof(yml_data), "w");
    if(!stream) {
        _log_error("ymldb:write: unable fmemopen stream\n");
        return -1;
    }
    _ymldb_dump_start(stream, opcode, 0);
    va_start (args, num);
    for(i=0; i<num; i++) 
    {
        token = va_arg(args, char *);
        if(!token) {
            fprintf(stream, "\n");
            break;
        }
        if(i+1 < num) {

            fprintf(stream, "%.*s%s:\n", i * 2, gSpace, token);
        }
        else {
            fprintf(stream, "%.*s%s\n", i * 2, gSpace, token);
        }
    }
    va_end (args);
    _ymldb_dump_end(stream);
    fclose(stream);

    return _ymldb_push(cb, yml_data);
}


int main(int argc, char *argv[])
{
    int help = 0;
    int infile = 0;
    int outfile = 0;
    int k;
    char *infilename = "";
    char *outfilename = "";
    FILE *infp = NULL;
    FILE *outfp = NULL;
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

    _log_debug("ymldb created!!");
    if (strcmp(infilename, "stdin") == 0)
    {
        infp = stdin;
    }
    else if(strlen(infilename) > 0)
    {
        infd = open(infilename, O_RDONLY, 0644);
        if (infd < 0)
        {
            fprintf(stderr, "file open error. %s\n", strerror(errno));
            return 1;
        }

        // infp = fopen_from_fd(infd, "r");
        // if (!infp)
        // {
        //     fprintf(stderr, "infilename fdopen error. %s\n", strerror(errno));
        //     return 1;
        // }
    }
    

    if (strcmp(outfilename, "stdout") == 0)
    {
        outfp = stdout;
    }
    else if(strlen(outfilename) > 0)
    {
        outfd = open(outfilename, O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (outfd < 0)
        {
            fprintf(stderr, "outfilename file open error. %s\n", strerror(errno));
            return 1;
        }

        // outfp = fopen_from_fd(outfd, "w");
        // if (!outfp)
        // {
        //     fprintf(stderr, "outfile fdopen error. %s\n", strerror(errno));
        //     return 1;
        // }
    }


    struct ymldb_cb *cb = NULL;
    
    cb = ymldb_create_with_fd("me", "my-ymldb", infd, outfd);
    if (!cb)
        return -1;

    ymldb_construct(cb);
    
    ymldb_push(cb, YMLDB_OP_MERGE,
        "system:\n"
        "  product: %s\n"
        "  serial-number: %s\n",
        "G.FAST-HN5124D",
        "HN5124-S100213124"
        );
    ymldb_write(cb, YMLDB_OP_MERGE, 3, "system", "product", "abc");
    
    ymldb_dump(cb, cb->ydb->parent, 0, 0);

    ymldb_destroy(cb);

    // for debug
    _log_debug("\n\n  alloc_cnt %d\n", alloc_cnt);

    if (infp)
        fclose(infp);
    if (outfp)
        fclose(outfp);
    if (infd)
        close(infd);
    if (outfd)
        close(outfd);

    return 0;
}
