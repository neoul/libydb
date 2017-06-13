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
#define _log_debug(...)               \
    do                                \
    {                                 \
        fprintf(stdout, __VA_ARGS__); \
    } while (0)

#define _log_error(...)               \
    do                                \
    {                                 \
        fprintf(stderr, __VA_ARGS__); \
    } while (0)

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

void ymldb_node_merge_notify(struct ymldb *ydb)
{
    cp_list *ancestors = ymldb_traverse_ancestors(ydb);
    _log_debug("@@ merge-notify {\n");

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

struct ymldb *ymldb_node_merge(struct ymldb *parent, ymldb_type_t type, char *key, char *value)
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
notify_ydb:
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

void ymldb_node_delete(struct ymldb *parent, char *key)
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

struct ymldb *ymldb_merge(struct ymldb *p_ydb, yaml_document_t *document, int index, int p_index)
{

    yaml_node_t *node = NULL;
    node = yaml_document_get_node(document, index);
    if (!p_ydb)
        p_ydb = ymldb_node_merge(NULL, YMLDB_BRANCH, "top", NULL);
    if (!node)
        return p_ydb;

    switch (node->type)
    {
    case YAML_SEQUENCE_NODE:
    {
        yaml_node_item_t *item;
        // printf("SEQ c=%d p=%d\n", index, p_index);
        for (item = node->data.sequence.items.start;
             item < node->data.sequence.items.top; item++)
        {
            yaml_node_t *node = yaml_document_get_node(document, *item);
            char *key = (char *)node->data.scalar.value;
            if (node->type == YAML_SCALAR_NODE)
            {
                ymldb_node_merge(p_ydb, YMLDB_LEAFLIST, key, NULL);
            }
            else
            {
                ymldb_merge(p_ydb, document, *item, index);
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
            yaml_node_t *key_node = yaml_document_get_node(document, pair->key);
            yaml_node_t *value_node = yaml_document_get_node(document, pair->value);
            char *key = (char *)key_node->data.scalar.value;
            char *value = (char *)value_node->data.scalar.value;

            if (value_node->type == YAML_SCALAR_NODE)
            {
                // if(value[0]==0) {
                //     _log_debug("value is empty\n");
                // }
                // _log_debug("key %s, value %s (tag %s)\n", key, value, (char *)value_node->tag);
                ymldb_node_merge(p_ydb, YMLDB_LEAF, key, value);
            }
            else
            { // not leaf
                struct ymldb *ydb = NULL;
                // _log_debug("key %s, value -\n", key);
                ydb = ymldb_node_merge(p_ydb, YMLDB_BRANCH, key, NULL);
                ymldb_merge(ydb, document, pair->value, index);
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
    return p_ydb;
}

struct ymldb *ymldb_delete(struct ymldb *p_ydb, yaml_document_t *document, int index, int p_index)
{

    yaml_node_t *node = NULL;
    if (!p_ydb)
        return NULL;
    node = yaml_document_get_node(document, index);
    if (!node)
        return p_ydb;

    switch (node->type)
    {
    case YAML_SEQUENCE_NODE:
    {
        yaml_node_item_t *item;
        // printf("SEQ c=%d p=%d\n", index, p_index);
        for (item = node->data.sequence.items.start;
             item < node->data.sequence.items.top; item++)
        {
            yaml_node_t *node = yaml_document_get_node(document, *item);
            char *key = (char *)node->data.scalar.value;
            if (node->type == YAML_SCALAR_NODE)
            {
                ymldb_node_delete(p_ydb, key);
            }
            else
            {
                ymldb_delete(p_ydb, document, *item, index);
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
            yaml_node_t *key_node = yaml_document_get_node(document, pair->key);
            yaml_node_t *value_node = yaml_document_get_node(document, pair->value);
            char *key = (char *)key_node->data.scalar.value;

            if (value_node->type == YAML_SCALAR_NODE)
            {
                ymldb_node_delete(p_ydb, key);
            }
            else
            { // not leaf
                struct ymldb *ydb = NULL;
                _log_debug("key %s, value -\n", key);
                ydb = cp_avltree_get(p_ydb->children, key);
                ymldb_delete(ydb, document, pair->value, index);
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
    return p_ydb;
}

struct ymldb *ymldb_get(struct ymldb *p_ydb, yaml_document_t *document, int index, int p_index)
{

    yaml_node_t *node = NULL;
    if (!p_ydb)
        return NULL;
    node = yaml_document_get_node(document, index);
    if (!node)
        return p_ydb;

    switch (node->type)
    {
    case YAML_SEQUENCE_NODE:
    {
        yaml_node_item_t *item;
        // printf("SEQ c=%d p=%d\n", index, p_index);
        for (item = node->data.sequence.items.start;
             item < node->data.sequence.items.top; item++)
        {
            yaml_node_t *node = yaml_document_get_node(document, *item);
            if (node->type == YAML_SCALAR_NODE)
            {
                // ymldb_node_delete(p_ydb, key);
            }
            else
            {
                ymldb_get(p_ydb, document, *item, index);
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
            yaml_node_t *key_node = yaml_document_get_node(document, pair->key);
            yaml_node_t *value_node = yaml_document_get_node(document, pair->value);
            char *key = (char *)key_node->data.scalar.value;

            if (value_node->type == YAML_SCALAR_NODE)
            {
                // ymldb_node_delete(p_ydb, key);
            }
            else
            { // not leaf
                struct ymldb *ydb = NULL;
                _log_debug("key %s, value -\n", key);
                ydb = cp_avltree_get(p_ydb->children, key);
                ymldb_get(ydb, document, pair->value, index);
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
    return p_ydb;
}

int yaml_parser_error(yaml_parser_t *parser)
{
    /* Display a parser error message. */
    switch (parser->error)
    {
    case YAML_MEMORY_ERROR:
        fprintf(stderr, "Memory error: Not enough memory for parsing\n");
        break;

    case YAML_READER_ERROR:
        if (parser->problem_value != -1)
        {
            fprintf(stderr, "Reader error: %s: #%X at %zd\n", parser->problem,
                    parser->problem_value, parser->problem_offset);
        }
        else
        {
            fprintf(stderr, "Reader error: %s at %lu\n", parser->problem,
                    parser->problem_offset);
        }
        break;

    case YAML_SCANNER_ERROR:
        if (parser->context)
        {
            fprintf(stderr, "Scanner error: %s at line %lu, column %lu\n"
                            "%s at line %lu, column %lu\n",
                    parser->context,
                    parser->context_mark.line + 1, parser->context_mark.column + 1,
                    parser->problem, parser->problem_mark.line + 1,
                    parser->problem_mark.column + 1);
        }
        else
        {
            fprintf(stderr, "Scanner error: %s at line %lu, column %lu\n",
                    parser->problem, parser->problem_mark.line + 1,
                    parser->problem_mark.column + 1);
        }
        break;

    case YAML_PARSER_ERROR:
        if (parser->context)
        {
            fprintf(stderr, "Parser error: %s at line %lu, column %lu\n"
                            "%s at line %lu, column %lu\n",
                    parser->context,
                    parser->context_mark.line + 1, parser->context_mark.column + 1,
                    parser->problem, parser->problem_mark.line + 1,
                    parser->problem_mark.column + 1);
        }
        else
        {
            fprintf(stderr, "Parser error: %s at line %lu, column %lu\n",
                    parser->problem, parser->problem_mark.line + 1,
                    parser->problem_mark.column + 1);
        }
        break;

    case YAML_COMPOSER_ERROR:
        if (parser->context)
        {
            fprintf(stderr, "Composer error: %s at line %lu, column %lu\n"
                            "%s at line %lu, column %lu\n",
                    parser->context,
                    parser->context_mark.line + 1, parser->context_mark.column + 1,
                    parser->problem, parser->problem_mark.line + 1,
                    parser->problem_mark.column + 1);
        }
        else
        {
            fprintf(stderr, "Composer error: %s at line %lu, column %lu\n",
                    parser->problem, parser->problem_mark.line + 1,
                    parser->problem_mark.column + 1);
        }
        break;

    default:
        /* Couldn't happen. */
        fprintf(stderr, "Internal error\n");
        break;
    }
    return 0;
}

int yaml_emitter_error(yaml_emitter_t *emitter)
{
    switch (emitter->error)
    {
    case YAML_MEMORY_ERROR:
        fprintf(stderr, "Memory error: Not enough memory for emitting\n");
        break;

    case YAML_WRITER_ERROR:
        fprintf(stderr, "Writer error: %s\n", emitter->problem);
        break;

    case YAML_EMITTER_ERROR:
        fprintf(stderr, "Emitter error: %s\n", emitter->problem);
        break;

    default:
        /* Couldn't happen. */
        fprintf(stderr, "Internal error\n");
        break;
    }
    return 0;
}

void yaml_document_dump(yaml_document_t *document, FILE *in)
{
    if (!document)
        return;
    yaml_emitter_t emitter;
    /* Set the emitter parameters. */
    if (!yaml_emitter_initialize(&emitter))
        goto emitter_error;
    yaml_emitter_set_output_file(&emitter, in);
    yaml_emitter_set_canonical(&emitter, 0);
    yaml_emitter_set_unicode(&emitter, 0);
    if (!yaml_emitter_dump(&emitter, document))
        goto emitter_error;
    yaml_emitter_delete(&emitter);
    return;

emitter_error:
    yaml_emitter_error(&emitter);
    yaml_emitter_delete(&emitter);
    return;
}

int ymldb_construct(struct ymldb **pydb, FILE *in, FILE *out)
{
    static int sequence = 0;
    int done = 0;
    yaml_parser_t parser;
    yaml_document_t document;
    struct ymldb *ydb = *pydb;
    if(!pydb) {
        _log_error("invalid ymldb\n");
        return -1;
    }

    /* Clear the objects. */
    memset(&parser, 0, sizeof(parser));
    memset(&document, 0, sizeof(document));

    /* Set the parser parameters. */
    if (!yaml_parser_initialize(&parser))
        goto parser_error;

    yaml_parser_set_input_file(&parser, in);

    _log_debug("> start\n");
    while (!done)
    {
        char *op = YMLDB_TAG_OP_MERGE;
        yaml_node_t *yroot = NULL;
        /* Get the next event. */
        if (!yaml_parser_load(&parser, &document))
            goto parser_error;

        if (document.tag_directives.start != document.tag_directives.end)
        {
            yaml_tag_directive_t *tag;
            for (tag = document.tag_directives.start;
                 tag != document.tag_directives.end; tag++)
            {
                op = (char *)tag->handle;
            }
        }
        yroot = yaml_document_get_root_node(&document);
        if (yroot)
        {
            sequence++;
            _log_debug("%dth %s\n", sequence, op);
            if (strcmp(op, YMLDB_TAG_OP_MERGE) == 0)
            {
                ydb = ymldb_merge(ydb, &document, 1, 1);
            }
            else if (strcmp(op, YMLDB_TAG_OP_DELETE) == 0)
            {
                ydb = ymldb_delete(ydb, &document, 1, 1);
            }
            else if (strcmp(op, YMLDB_TAG_OP_GET) == 0)
            {
                ydb = ymldb_get(ydb, &document, 1, 1);
            }
            ymldb_dump_all(ydb);
        }
        else
        { /* Check if this is the in end. */
            done = 1;
        }
        // yaml_document_dump(&document, stdout);
        yaml_document_delete(&document);
    }
    _log_debug("> end\n\n");
    yaml_parser_delete(&parser);
    *pydb = ydb;
    return 0;

parser_error:
    yaml_parser_error(&parser);
    yaml_parser_delete(&parser);
    *pydb = ydb;
    return -1;
}

void ymldb_destroy(struct ymldb **pydb)
{
    struct ymldb *ydb = *pydb;
    while (ydb)
    {
        if (ydb->parent)
            ydb = ydb->parent;
        else
            break;
    }
    _ymldb_node_free(ydb);
    return;
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

    struct ymldb *ydb = NULL;
    ymldb_construct(&ydb, infp, outfp);
    ymldb_dump_all(ydb);
    ymldb_destroy(&ydb);

    // for debug
    _log_debug("\n\n  alloc_cnt %d\n", alloc_cnt);

    fclose(infp);
    close(fd);

    return 0;
}
