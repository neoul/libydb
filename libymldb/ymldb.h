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
#define YMLDB_STREAM_THRESHOLD 2048
#define YMLDB_STREAM_BUF_SIZE (YMLDB_STREAM_THRESHOLD + 512)

#define YMLDB_SUBSCRIBER_MAX 8
struct ymldb_cb
{
    char *key;
    struct ymldb *ydb;
    struct ymldb *last_notify; // last updated ydb
    yaml_document_t *document;
    unsigned int sequence;
    unsigned int opcode;
    unsigned int flags;
    struct
    {
        FILE *stream;
        char *buf;
        size_t buflen;
        int no_reply;
    } reply;
    // int fd_local; // fd for YMLDB_FLAG_LOCAL
    int fd_publisher;                        // fd for YMLDB_FLAG_PUBLISHER and YMLDB_FLAG_SUBSCRIBER
    int fd_subscriber[YMLDB_SUBSCRIBER_MAX]; // fd for YMLDB_FLAG_PUBLISHER
    struct
    {
        FILE *stream;
        int res;
    } out;
};

#define YMLDB_TAG_OP_GET "!get!"
#define YMLDB_TAG_OP_DELETE "!delete!"
#define YMLDB_TAG_OP_MERGE "!merge!"
#define YMLDB_TAG_OP_SUBSCRIBER "!subscriber!"
#define YMLDB_TAG_OP_PUBLISHER "!publisher!"
#define YMLDB_TAG_OP_LOCAL "!local!"
#define YMLDB_TAG_OP_REMOTE "!remote!"

#define YMLDB_TAG_BASE "ymldb:op:"
#define YMLDB_TAG_GET YMLDB_TAG_BASE "get"
#define YMLDB_TAG_DELETE YMLDB_TAG_BASE "delete"
#define YMLDB_TAG_MERGE YMLDB_TAG_BASE "merge"
#define YMLDB_TAG_SUBSCRIBER YMLDB_TAG_BASE "subscriber"
#define YMLDB_TAG_PUBLISHER YMLDB_TAG_BASE "publisher"
#define YMLDB_TAG_LOCAL YMLDB_TAG_BASE "local"
#define YMLDB_TAG_REMOTE YMLDB_TAG_BASE "remote"

// opcode
#define YMLDB_OP_GET 0x01
#define YMLDB_OP_DELETE 0x02
#define YMLDB_OP_MERGE 0x04
#define YMLDB_OP_SUBSCRIBER 0x10
#define YMLDB_OP_PUBLISHER 0x20
#define YMLDB_OP_LOCAL 0x40
#define YMLDB_OP_REMOTE 0x80

// flags
#define YMLDB_FLAG_NONE 0x0
#define YMLDB_FLAG_PUBLISHER YMLDB_OP_PUBLISHER // publish ymldb if set, subscribe ymldb if not.
#define YMLDB_FLAG_SUBSCRIBER YMLDB_OP_SUBSCRIBER
#define YMLDB_FLAG_CONN (YMLDB_FLAG_PUBLISHER | YMLDB_FLAG_SUBSCRIBER) // communcation channel enabled ()
#define YMLDB_FLAG_RECONNECT 0x100
// #define YMLDB_FLAG_LOCAL        0x08

#define YMLDB_UNIXSOCK_PATH "@ymldb:%s"

void ymldb_dump_all(FILE *stream);
void ymldb_dump(FILE *stream, struct ymldb *ydb, int print_level, int no_print_children);
void ymldb_dump_start(FILE *stream, int opcode, int sequence);
void ymldb_dump_end(FILE *stream);

struct ymldb_cb *ymldb_create(char *key, unsigned int flags);
void ymldb_destroy(struct ymldb_cb *cb);

int ymldb_push(struct ymldb_cb *cb, int opcode, char *format, ...);
int _ymldb_write(struct ymldb_cb *cb, int opcode, int num, ...);
#define ymldb_write(CB, NUM, ...) _ymldb_write(CB, YMLDB_OP_MERGE, NUM, __VA_ARGS__)
#define ymldb_delete(CB, NUM, ...) _ymldb_write(CB, YMLDB_OP_DELETE, NUM, __VA_ARGS__)
int ymldb_pull(struct ymldb_cb *cb, char *format, ...);
int ymldb_run(struct ymldb_cb *cb, int infd, int outfd);

int ymldb_conn_deinit(struct ymldb_cb *cb);
int ymldb_conn_init(struct ymldb_cb *cb, int flags);
int ymldb_conn_set(struct ymldb_cb *cb, fd_set *set);
int ymldb_conn_recv(struct ymldb_cb *cb, fd_set *set);

int ymldb_local_init(struct ymldb_cb *cb, int fd);
int ymldb_local_deinit(struct ymldb_cb *cb);

#define _log_printf(...)               \
    do                                \
    {                                 \
        fprintf(stdout, "\n");        \
        fprintf(stdout, __VA_ARGS__); \
    } while (0)

#define _log_debug(...)                                                   \
    do                                                                    \
    {                                                                     \
        fprintf(stdout, "[ymldb:debug] %s:%d: ", __FUNCTION__, __LINE__); \
        fprintf(stdout, __VA_ARGS__);                                     \
    } while (0)

#define _log_error(...)                                             \
    do                                                              \
    {                                                               \
        fprintf(stderr, "\n[ymldb:error]\n\n");                     \
        fprintf(stderr, "  - %s:%d\n  - ", __FUNCTION__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                               \
        fprintf(stderr, "\n");                                      \
    } while (0)
#endif