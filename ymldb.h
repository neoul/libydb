#ifndef __YMLDB__
#define __YMLDB__


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
    struct ymldb *last_notify; // last ancestor ydb
    int sequence;
    yaml_parser_t *parser;
    yaml_emitter_t *emitter;
    yaml_document_t *in_document;
    yaml_document_t *out_document;
    FILE *in;
    FILE *out;
};


void ymldb_dump(struct ymldb_cb *cb, struct ymldb *ydb, int print_level, int no_print_children);
cp_list *ymldb_traverse_ancestors(struct ymldb *ydb, int traverse_level);
void ymldb_traverse_free(cp_list *templist);
#endif