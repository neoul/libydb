#include <stdlib.h>
#include <stdio.h>
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
        /*fprintf(stdout, "%.*s: ", 16, __FUNCTION__);*/ \
        fprintf(stdout, __VA_ARGS__);                    \
    } while (0)

#define _log_error(...)                                  \
    do                                                   \
    {                                                    \
        /*fprintf(stdout, "%.*s: ", 16, __FUNCTION__);*/ \
        fprintf(stderr, __VA_ARGS__);                    \
    } while (0)

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

// fdopen !!

#define YMLDB_TAG_OP_GET "!get!"
#define YMLDB_TAG_OP_DELETE "!delete!"
#define YMLDB_TAG_OP_MERGE "!merge!"
#define YMLDB_TAG_BASE "actusnetworks.com:op:"
#define YMLDB_TAG_GET YMLDB_TAG_BASE "get"
#define YMLDB_TAG_DELETE YMLDB_TAG_BASE "delete"
#define YMLDB_TAG_MERGE YMLDB_TAG_BASE "merge"

typedef int ymldb_hooker(void *);

char *_dump_str(const char *src)
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

char *_dump_yaml_node_type(yaml_node_type_t t)
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

typedef enum ymldb_type_e {
    YMLDB_LEAF,
    YMLDB_LEAFLIST,
    YMLDB_BRANCH
} ymldb_type_t;

struct ymldb
{
    char *key;
    ymldb_type_t type;
    union {
        cp_avltree *children;
        char *value;
    };
    struct ymldb *parent;
    int level;
};

// ymldb control block
struct ymldb_cb
{
    struct ymldb *ydb;
    int sequence;
    yaml_parser_t *parser;
    yaml_emitter_t *emitter;
    yaml_document_t *in_document;
    yaml_document_t *out_document;
    FILE *in;
    FILE *out;
    cp_list *last_changed;
};

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
    if(!emitter)
        return;
    if(!document)
        return;
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
    if(!document) {
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
    if(!parser) {
        _log_error("ymldb:document: invalid parser.\n");
        return NULL;
    }

    document = malloc(sizeof(yaml_document_t));
    if(!document) {
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
    if(!in) {
        _log_error("ymldb:parser: invalid in.\n");
        return NULL;
    }
    parser = malloc(sizeof(yaml_parser_t));
    if(!parser) {
        _log_error("ymldb:parser: memory alloc failed.\n");
        return NULL;
    }
    if (!yaml_parser_initialize(parser)) {
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
    if(!out) {
        _log_error("ymldb: invalid out.\n");
        return NULL;
    }
    emitter = malloc(sizeof(yaml_emitter_t));
    if(!emitter) {
        _log_error("ymldb:emitter: memory alloc failed.\n");
        return NULL;
    }
    if (!yaml_emitter_initialize(emitter)) {
        _log_error_ydb_emitter(emitter);
        yaml_emitter_free(emitter);
        return NULL;
    }
    yaml_emitter_set_output_file(emitter, out);
    yaml_emitter_set_canonical(emitter, 0);
    yaml_emitter_set_unicode(emitter, 0);
    return emitter;
}

int yaml_document_update(yaml_document_t *document, struct ymldb *prev, struct ymldb *cur)
{
    // compare previous and current ydb.
    // update the current ydb below the previous ydb in the document.
    if(!prev) goto doc_update;

doc_update:
    return 0;
}

static int ymldb_dump(void *n, void *dummy)
{
    cp_avlnode *node = n;
    struct ymldb *ydb = node->value;
    char indentstr[32];
    memset(indentstr, ' ', sizeof(indentstr));
    if (ydb->type == YMLDB_BRANCH)
    {
        _log_debug("%.*s+ %s\n", ydb->level, indentstr, _dump_str(ydb->key));
        cp_avltree_callback(ydb->children, ymldb_dump, NULL);
    }
    else if (ydb->type == YMLDB_LEAFLIST)
        _log_debug("%.*s- %s\n", ydb->level, indentstr, _dump_str(ydb->value));
    else
        _log_debug("%.*s* %s: '%s'\n", ydb->level, indentstr, _dump_str(ydb->key), _dump_str(ydb->value));
    return 0;
}

static void ymldb_dump_all(struct ymldb *ydb)
{
    char indentstr[32];
    if (!ydb)
        return;
    memset(indentstr, ' ', sizeof(indentstr));
    if (ydb->type == YMLDB_BRANCH)
    {
        _log_debug("%.*s+ %s\n", ydb->level, indentstr, _dump_str(ydb->key));
        cp_avltree_callback(ydb->children, ymldb_dump, NULL);
    }
    else if (ydb->type == YMLDB_LEAFLIST)
        _log_debug("%.*s- %s\n", ydb->level, indentstr, _dump_str(ydb->value));
    else
        _log_debug("%.*s* %s: '%s'\n", ydb->level, indentstr, _dump_str(ydb->key), _dump_str(ydb->value));
    _log_debug("\n");
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

cp_list *ymldb_traverse_ancestors(struct ymldb *ydb)
{
    if (!ydb)
        return NULL;
    cp_list *templist = cp_list_create_nosync();
    while (ydb)
    {
        cp_list_insert(templist, ydb->key);
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

char *space= "                                                                                ";
void ymldb_node_merge_notify(struct ymldb *ydb)
{
    cp_list *templist = cp_list_create_nosync();
    while (ydb)
    {
        cp_list_insert(templist, ydb);
        ydb = ydb->parent;
    }
    if(cp_list_is_empty(templist))
        return;
    struct ymldb *cur;
    cp_list_iterator iter;
    cp_list_iterator_init(&iter, templist, COLLECTION_LOCK_NONE);
    while ((cur = cp_list_iterator_next(&iter)))
    {
        if(strcmp("top", cur->key) == 0)
            continue;
        switch(cur->type)
        {
            case YMLDB_BRANCH:
            _log_debug("%.*s%s:\n", (cur->level-1)*2, space, cur->key);
            break;
            case YMLDB_LEAFLIST:
            _log_debug("%.*s- %s\n", (cur->level-1)*2, space, cur->key);
            break;
            case YMLDB_LEAF:
            _log_debug("%.*s%s: %s\n", (cur->level-1)*2, space, cur->key, cur->value);
            break;
            default:
            break;
        }
    }
    cp_list_destroy(templist);
    _log_debug("!!!\n");
    ymldb_dump_all(ydb);
    _log_debug("!!!\n");


    // cp_list *ancestors = ymldb_traverse_ancestors(ydb);
    // _log_debug("@@ merge-notify {\n");

    // char *keystr;
    // cp_list_iterator iter;
    // cp_list_iterator_init(&iter, ancestors, COLLECTION_LOCK_NONE);
    // while ((keystr = cp_list_iterator_next(&iter)))
    // {
    //     _print_item(keystr, NULL);
    // }
    // _log_debug("}\n\n");

    // ymldb_traverse_free(ancestors);
    return;
}

void ymldb_node_delete_notify(struct ymldb *parent, char *key)
{
    cp_list *ancestors = NULL;
    struct ymldb *ydb = cp_avltree_get(parent->children, key);
    if (!ydb)
    {
        // do nothing if not exist.
        return;
    }
    ancestors = ymldb_traverse_ancestors(ydb);
    _log_debug("@@ delete-notify {\n");
    char *keystr;
    cp_list_iterator iter;
    cp_list_iterator_init(&iter, ancestors, COLLECTION_LOCK_NONE);
    while ((keystr = cp_list_iterator_next(&iter)))
    {
        _print_item(keystr, NULL);
    }
    _log_debug("}\n\n");

    ymldb_traverse_free(ancestors);
    return;
}

void ymldb_node_get_reply(struct ymldb *parent, char *key)
{
    cp_list *ancestors = NULL;
    struct ymldb *ydb = cp_avltree_get(parent->children, key);
    if (!ydb)
    {
        // do nothing if not exist.
        return;
    }
    ancestors = ymldb_traverse_ancestors(ydb);
    _log_debug("@@ delete-notify {\n");
    char *keystr;
    cp_list_iterator iter;
    cp_list_iterator_init(&iter, ancestors, COLLECTION_LOCK_NONE);
    while ((keystr = cp_list_iterator_next(&iter)))
    {
        _print_item(keystr, NULL);
    }
    _log_debug("}\n\n");

    ymldb_traverse_free(ancestors);
    return;
}

struct ymldb *ymldb_node_merge(struct ymldb_cb *cb, struct ymldb *parent, ymldb_type_t type, char *key, char *value)
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
                    ymldb_node_merge_notify(ydb);
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
    ymldb_node_merge_notify(ydb);
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

void ymldb_node_delete(struct ymldb_cb *cb, struct ymldb *parent, char *key)
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
        ymldb_node_delete_notify(parent, key);
        ydb = cp_avltree_delete(parent->children, key);
        _ymldb_node_free(ydb);
    }
    return;
}

void ymldb_node_get(struct ymldb_cb *cb, struct ymldb *parent, char *key)
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
        ymldb_node_get_reply(parent, key);
    }
    return;
}

int ymldb_merge(struct ymldb_cb *cb, struct ymldb *p_ydb, int index, int p_index)
{
    yaml_node_t *node = NULL;
    node = yaml_document_get_node(cb->in_document, index);
    if (!p_ydb)
        return -1;

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
                ymldb_node_merge(cb, p_ydb, YMLDB_LEAFLIST, key, NULL);
            }
            else
            {
                ymldb_merge(cb, p_ydb, *item, index);
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
                // if(value[0]==0) {
                //     _log_debug("value is empty\n");
                // }
                // _log_debug("key %s, value %s (tag %s)\n", key, value, (char *)value_node->tag);
                ymldb_node_merge(cb, p_ydb, YMLDB_LEAF, key, value);
            }
            else
            { // not leaf
                struct ymldb *ydb = NULL;
                // _log_debug("key %s, value -\n", key);
                ydb = ymldb_node_merge(cb, p_ydb, YMLDB_BRANCH, key, NULL);
                ymldb_merge(cb, ydb, pair->value, index);
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

int ymldb_delete(struct ymldb_cb *cb, struct ymldb *p_ydb, int index, int p_index)
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
                ymldb_node_delete(cb, p_ydb, key);
            }
            else
            {
                ymldb_delete(cb, p_ydb, *item, index);
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
                ymldb_node_delete(cb, p_ydb, key);
            }
            else
            { // not leaf
                struct ymldb *ydb = NULL;
                _log_debug("key %s, value -\n", key);
                ydb = cp_avltree_get(p_ydb->children, key);
                ymldb_delete(cb, ydb, pair->value, index);
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

int ymldb_get(struct ymldb_cb *cb, struct ymldb *p_ydb, int index, int p_index)
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
                ymldb_node_get(cb, p_ydb, key);
            }
            else
            {
                ymldb_get(cb, p_ydb, *item, index);
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
                ymldb_node_get(cb, p_ydb, key);
            }
            else
            { // not leaf
                struct ymldb *ydb = NULL;
                _log_debug("key %s\n", key);
                ydb = cp_avltree_get(p_ydb->children, key);
                ymldb_get(cb, ydb, pair->value, index);
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

int ymldb_construct(struct ymldb_cb *cb, FILE *in, FILE *out)
{
    int done = 0;
    if (!cb)
    {
        _log_error("ymldb:construct:invalid ymldb_cb\n");
        return -1;
    }
    if (!in) {
        _log_error("ymldb:construct:invalid input stream\n");
        return -1;
    }

    cb->in = in;
    cb->out = out;

    cb->parser = yaml_parser_init(in);
    if(!cb->parser) {
        _log_error("ymldb:construct:parser init failed\n");
        return -1;
    }

    _log_debug("> start\n");
    while (!done)
    {
        char *op = YMLDB_TAG_OP_MERGE;
        yaml_node_t *yroot = NULL;
        /* Get the next ymldb document. */
        cb->in_document = yaml_document_load(cb->parser);
        if(!cb->in_document) {
            _log_error("ymldb:construct:load a document from parser - failed\n");
            break;
        }

        if (cb->in_document->tag_directives.start != cb->in_document->tag_directives.end)
        {
            yaml_tag_directive_t *tag;
            for (tag = cb->in_document->tag_directives.start;
                 tag != cb->in_document->tag_directives.end; tag++)
            {
                op = (char *)tag->handle;
            }
        }

        yroot = yaml_document_get_root_node(cb->in_document);
        if (yroot)
        {
            cb->sequence++;
            _log_debug("%dth %s\n", cb->sequence, op);
            
            if(out) {
                cb->emitter = yaml_emitter_init(out);
            }
            cb->out_document = yaml_document_init(op);
           
            if (strcmp(op, YMLDB_TAG_OP_MERGE) == 0)
            {
                ymldb_merge(cb, cb->ydb, 1, 1);
            }
            else if (strcmp(op, YMLDB_TAG_OP_DELETE) == 0)
            {
                ymldb_delete(cb, cb->ydb, 1, 1);
            }
            else if (strcmp(op, YMLDB_TAG_OP_GET) == 0)
            {
                ymldb_get(cb, cb->ydb, 1, 1);
            }

            yaml_emitter_flush_and_free(cb->emitter, cb->out_document);
            cb->out_document = NULL;
            cb->emitter = NULL;
        }
        else
        { /* Check if this is the in end. */
            done = 1;
        }

        yaml_document_free(cb->in_document);
        cb->in_document = NULL;
    }
    _log_debug("> end\n\n");
    yaml_parser_free(cb->parser);
    cb->parser = NULL;
    cb->in = NULL;
    cb->out = NULL;
    return 0;
}

struct ymldb_cb *ymldb_init()
{
    struct ymldb_cb *cb = malloc(sizeof(struct ymldb_cb));
    if (!cb) {
        _log_error("ymldb:init failed.\n");
        return NULL;
    }
    cb->parser = NULL;
    cb->in_document = NULL;
    cb->out_document = NULL;
    cb->emitter = NULL;
    cb->sequence = 0;
    cb->in = NULL;
    cb->out = NULL;
    cb->ydb = ymldb_node_merge(cb, NULL, YMLDB_BRANCH, "top", NULL);
    if(!cb->ydb) {
        _log_error("ymldb:top init failed.\n");
        free(cb);
        return NULL;
    }
    return cb;
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
    if (cb->ydb) {
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
    free(cb);
}

int main(int argc, char *argv[])
{
    int help = 0;
    int canonical = 0;
    int unicode = 0;
    int file = 0;
    int k;
    char *filename = NULL;
    FILE *infp = NULL;
    FILE *outfp = NULL;
    int fd = 0;

    /* Analyze command line options. */
    for (k = 1; k < argc; k++)
    {
        // fprintf(stdout, "argv[%d]=%s\n", k, argv[k]);
        if (strcmp(argv[k], "-h") == 0 || strcmp(argv[k], "--help") == 0)
        {
            help = 1;
        }
        else if (strcmp(argv[k], "-c") == 0 || strcmp(argv[k], "--canonical") == 0)
        {
            canonical = 1;
        }
        else if (strcmp(argv[k], "-u") == 0 || strcmp(argv[k], "--unicode") == 0)
        {
            unicode = 1;
        }
        else if (strcmp(argv[k], "-f") == 0 || strcmp(argv[k], "--file") == 0)
        {
            file = 1;
        }
        else
        {
            if (file)
            {
                filename = argv[k];
                fprintf(stderr, "finename %s\n", filename);
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
    _log_debug("help %d canonical %d unicode %d file %d\n", help, canonical, unicode, file);

    /* Display the help string. */
    if (help)
    {
        printf("%s <input\n"
               "or\n%s -h | --help\nDeconstruct a YAML in\n\nOptions:\n"
               "-h, --help\t\tdisplay this help and exit\n"
               "-c, --canonical\t\toutput in the canonical YAML format\n"
               "-u, --unicode\t\toutput unescaped non-ASCII characters\n"
               "-f, --file\t\tinput ymldb file to read. (*.yml)\n",
               argv[0], argv[0]);
        return 0;
    }

    if (file)
    {
        if (filename)
        {
            fd = open(filename, O_RDONLY, 0644);
            if (fd < 0)
            {
                fprintf(stderr, "file open error. %s\n", strerror(errno));
                return 1;
            }

            int dup_fd = dup(fd);
            infp = fdopen(dup_fd, "r");
            if (!infp)
            {
                fprintf(stderr, "fdopen error. %s\n", strerror(errno));
                return 1;
            }
        }
        else
        {
            fprintf(stderr, "No filename configured.\n");
            return 1;
        }
    }
    else
    {
        fd = 1;
        infp = stdin;
    }

    outfp = stdout;

    struct ymldb_cb *cb = NULL;
    cb = ymldb_init();
    if (!cb)
         return -1;
 
    ymldb_construct(cb, infp, outfp);

    ymldb_dump_all(cb->ydb);

    ymldb_destroy(cb);

    // for debug
    _log_debug("\n\n  alloc_cnt %d\n", alloc_cnt);

    fclose(infp);
    close(fd);

    return 0;
}
