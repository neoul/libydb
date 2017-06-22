#ifndef __YMLDB__
#define __YMLDB__
#include <yaml.h>
#include <cprops/avl.h>
#include <cprops/linked_list.h>

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
#define YMLDB_SUBSCRIBER_MAX 8
struct ymldb_cb
{
    char *key;
    struct ymldb *ydb;
    struct ymldb *last_notify; // last updated ydb
    yaml_parser_t *parser;
    yaml_document_t *document;
    unsigned int sequence;
    unsigned int opcode;
    unsigned int flags;
    FILE *out;
    int publisher; // fd
    int subscriber[YMLDB_SUBSCRIBER_MAX]; // fd
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
#define YMLDB_OP_GET 0x01
#define YMLDB_OP_DELETE 0x02
#define YMLDB_OP_MERGE 0x04
#define YMLDB_OP_SUBSCRIBE 0x08
#define YMLDB_OP_UNSUBSCRIBE 0x10
#define YMLDB_OP_PUBLISH 0x20

// flags
#define YMLDB_FLAG_FD           0x01 // communcation channel enabled
#define YMLDB_FLAG_PUBLISHER    0x02 // publish ymldb if set, subscribe ymldb if not.
#define YMLDB_FLAG_RECONNECT    0x04

#define YMLDB_UNIXSOCK_PATH "@ymldb:%s"

void print_alloc_cnt();
void ymldb_dump(struct ymldb_cb *cb, struct ymldb *ydb, int print_level, int no_print_children);

int ymldb_run(struct ymldb_cb *cb, FILE *instream);
struct ymldb_cb *ymldb_create(char *key, FILE *out);
struct ymldb_cb *ymldb_create_with_fd(char *key, int outfd);
void ymldb_destroy(struct ymldb_cb *cb);
int ymldb_push(struct ymldb_cb *cb, int opcode, char *format, ...);
int _ymldb_write(struct ymldb_cb *cb, int opcode, int num, ...);
#define ymldb_write(CB, NUM, ...) _ymldb_write(CB, YMLDB_OP_MERGE, NUM, __VA_ARGS__)
#define ymldb_delete(CB, NUM, ...) _ymldb_write(CB, YMLDB_OP_DELETE, NUM, __VA_ARGS__)


int ymldb_fd_deinit(struct ymldb_cb *cb);
int ymldb_fd_init(struct ymldb_cb *cb, int flags);
int ymldb_fd_set(struct ymldb_cb *cb, fd_set *set);
int ymldb_fd_run(struct ymldb_cb *cb, fd_set *set);

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

#endif