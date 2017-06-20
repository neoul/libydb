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
    char *key;
    char *owner;
    struct ymldb *ydb;
    struct ymldb *last_notify; // last updated ydb
    int sequence;
    yaml_parser_t *parser;
    yaml_emitter_t *emitter;
    yaml_document_t *in_document;
    yaml_document_t *out_document;
    FILE *in;
    FILE *out;
    int opcode;
};

#define YMLDB_TAG_OP_GET "!get!"
#define YMLDB_TAG_OP_DELETE "!delete!"
#define YMLDB_TAG_OP_MERGE "!merge!"
#define YMLDB_TAG_OP_SUBSCRIBE "!subscribe!"
#define YMLDB_TAG_OP_UNSUBSCRIBE "!unsubscribe!"
#define YMLDB_TAG_OP_PUBLISH "!publish!"

#define YMLDB_TAG_BASE "actusnetworks.com:op:"
#define YMLDB_TAG_GET YMLDB_TAG_BASE "get"
#define YMLDB_TAG_DELETE YMLDB_TAG_BASE "delete"
#define YMLDB_TAG_MERGE YMLDB_TAG_BASE "merge"
#define YMLDB_TAG_SUBSCRIBE YMLDB_TAG_BASE "subscribe"
#define YMLDB_TAG_UNSUBSCRIBE YMLDB_TAG_BASE "unsubscribe"
#define YMLDB_TAG_PUBLISH YMLDB_TAG_BASE "publish"

// opcode
#define YMLDB_OP_GET            0x01
#define YMLDB_OP_DELETE         0x02
#define YMLDB_OP_MERGE          0x04
#define YMLDB_OP_SUBSCRIBE      0x08
#define YMLDB_OP_UNSUBSCRIBE    0x10
#define YMLDB_OP_PUBLISH        0x20

void _alloc_cnt();
void ymldb_dump(struct ymldb_cb *cb, struct ymldb *ydb, int print_level, int no_print_children);

int ymldb_construct(struct ymldb_cb *cb);
struct ymldb_cb *ymldb_create(char *owner, char *key, FILE *in, FILE *out);
struct ymldb_cb *ymldb_create_with_fd(char *owner, char *key, int infd, int outfd);
void ymldb_destroy(struct ymldb_cb *cb);
int ymldb_push (struct ymldb_cb *cb, int opcode, char * format, ...);
int ymldb_write(struct ymldb_cb *cb, int opcode, int num, ...);
#endif