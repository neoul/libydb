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

// ymldb stream buffer
struct ymldb_stream
{
    FILE *stream;
    size_t buflen;
    char buf[];
};

#define YMLDB_STREAM_THRESHOLD 1536
#define YMLDB_STREAM_BUF_SIZE (YMLDB_STREAM_THRESHOLD + 512)

// ymldb control block
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
#define YMLDB_TAG_OP_SYNC "!sync!"
#define YMLDB_TAG_OP_SEQ "!seq!"

#define YMLDB_TAG_BASE "ymldb:op:"
#define YMLDB_TAG_GET YMLDB_TAG_BASE "get"
#define YMLDB_TAG_DELETE YMLDB_TAG_BASE "delete"
#define YMLDB_TAG_MERGE YMLDB_TAG_BASE "merge"
#define YMLDB_TAG_SUBSCRIBER YMLDB_TAG_BASE "subscriber"
#define YMLDB_TAG_PUBLISHER YMLDB_TAG_BASE "publisher"
#define YMLDB_TAG_SYNC YMLDB_TAG_BASE "sync"
#define YMLDB_TAG_SEQ "ymldb:seq:"

// opcode
#define YMLDB_OP_GET 0x01
#define YMLDB_OP_DELETE 0x02
#define YMLDB_OP_MERGE 0x04
#define YMLDB_OP_SEQ 0x08
#define YMLDB_OP_SUBSCRIBER 0x10
#define YMLDB_OP_PUBLISHER 0x20
#define YMLDB_OP_SYNC 0x40

#define YMLDB_OP_ACTION (YMLDB_OP_GET | YMLDB_OP_DELETE | YMLDB_OP_MERGE | YMLDB_OP_SYNC)

// flags
#define YMLDB_FLAG_NONE 0x00
#define YMLDB_FLAG_PUBLISHER 0x01 // publish ymldb if set, subscribe ymldb if not.
#define YMLDB_FLAG_SUBSCRIBER 0x02
#define YMLDB_FLAG_CONN (YMLDB_FLAG_PUBLISHER | YMLDB_FLAG_SUBSCRIBER) // communcation channel enabled
#define YMLDB_FLAG_NOSYNC 0x04
#define YMLDB_FLAG_RECONNECT 0x100

#define YMLDB_UNIXSOCK_PATH "@ymldb:%s"

// print all or partial ymldb to stream.
void ymldb_dump_all(FILE *stream);
void ymldb_dump(FILE *stream, struct ymldb *ydb, int print_level, int no_print_children);
void ymldb_dump_start(FILE *stream, unsigned int opcode, unsigned int sequence);
void ymldb_dump_end(FILE *stream);

// create or delete ymldb
struct ymldb_cb *ymldb_create(char *key, unsigned int flags);
void ymldb_destroy(struct ymldb_cb *cb);
struct ymldb_cb *ymldb_cb(char *key);

// basic functions to update ymldb
int _ymldb_push(struct ymldb_cb *cb, FILE *outstream, unsigned int opcode, char *format, ...);
int _ymldb_write(struct ymldb_cb *cb, FILE *outstream, unsigned int opcode, ...);
char *_ymldb_read(struct ymldb_cb *cb, ...);

// wrapping functions for ymldb api
int ymldb_push(struct ymldb_cb *cb, char *format, ...);

#define ymldb_write(CB, ...) _ymldb_write(CB, NULL, YMLDB_OP_MERGE, __VA_ARGS__, NULL)
#define ymldb_delete(CB, ...) _ymldb_write(CB, NULL, YMLDB_OP_DELETE, __VA_ARGS__, NULL)
#define ymldb_get(CB, OUTPUT, ...) _ymldb_write(CB, OUTPUT, YMLDB_OP_GET, __VA_ARGS__, NULL)
#define ymldb_sync(CB, ...) _ymldb_write(CB, NULL, YMLDB_OP_SYNC, __VA_ARGS__, NULL)

// only support to query a value using a key.
// char *product = ymldb_read(cb, "system", "product");
#define ymldb_read(CB, ...) _ymldb_read(CB, __VA_ARGS__, NULL)
int ymldb_pull(struct ymldb_cb *cb, char *format, ...);

// used to update ymldb using file descriptors.
int ymldb_run(struct ymldb_cb *cb, int infd, int outfd);

int ymldb_conn_deinit(struct ymldb_cb *cb);
int ymldb_conn_init(struct ymldb_cb *cb, int flags);
int ymldb_conn_set(struct ymldb_cb *cb, fd_set *set);
int ymldb_conn_recv(struct ymldb_cb *cb, fd_set *set);


#define _log_printf(...)              \
    do                                \
    {                                 \
        fprintf(stdout, __VA_ARGS__); \
    } while (0)

#define _log_debug(...)                                                   \
    do                                                                    \
    {                                                                     \
        fprintf(stdout, "[ymldb:debug] %s:%d: ", __FUNCTION__, __LINE__); \
        fprintf(stdout, __VA_ARGS__);                                     \
    } while (0)

#define _log_error(...)                                         \
    do                                                          \
    {                                                           \
        fprintf(stderr, "\n[ymldb:error]\n\n");                 \
        fprintf(stderr, "\t%s:%d\n\t", __FUNCTION__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                           \
        fprintf(stderr, "\n");                                  \
    } while (0)

#define _log_error_head(...)                                    \
    do                                                          \
    {                                                           \
        fprintf(stderr, "\n[ymldb:error]\n\n");                 \
        fprintf(stderr, "\t%s:%d\n\t", __FUNCTION__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                           \
    } while (0)

#define _log_error_next(...)                           \
    do                                                 \
    {                                                  \
        fprintf(stderr, "\t", __FUNCTION__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                  \
    } while (0)

#endif