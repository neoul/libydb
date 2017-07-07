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

typedef int (*ymldb_callback_fn)(void *usr_data, int deleted);

struct ymldb_callback
{
    ymldb_callback_fn usr_func;
    void *usr_data;
    int deleted;
};

struct ymldb
{
    char *key;
    int level;
    ymldb_type_t type;
    union {
        cp_avltree *children;
        char *value;
    };
    struct ymldb *parent;
    struct ymldb_callback *callback;
};

// #define OPEN_MEMSTREAM_ENABED
struct ymldb_stream
{
    FILE *stream;
    size_t buflen;
    size_t len;
    int is_write;
#ifdef OPEN_MEMSTREAM_ENABED
    char *buf;
#else
    char buf[];
#endif
};


#define YMLDB_STREAM_THRESHOLD 1536
#define YMLDB_STREAM_BUF_SIZE (YMLDB_STREAM_THRESHOLD + 512)

// ymldb control block
#define YMLDB_SUBSCRIBER_MAX 8
struct ymldb_cb
{
    char *key;
    struct ymldb *ydb;
    unsigned int flags;
    // fd for YMLDB_FLAG_PUBLISHER and YMLDB_FLAG_SUBSCRIBER
    int fd_publisher;
    // fd for YMLDB_FLAG_PUBLISHER
    int fd_subscriber[YMLDB_SUBSCRIBER_MAX];
};

struct ymldb_distribution
{
    fd_set *set;
    int max;
};

struct ymldb_params
{
    struct ymldb_cb *cb;
    yaml_parser_t parser;
    yaml_document_t document;
    struct
    {
        unsigned int opcode;
        unsigned int sequence;
        FILE *stream;
    } in;
    struct
    {
        unsigned int opcode;
        unsigned int sequence;
        FILE *stream;
    } out;
    int res;
    int no_reply;
    int no_change;
    struct ymldb *last_ydb; // last updated ydb
    cp_avltree *callbacks;
    struct ymldb_stream *streambuffer;
};

// yaml tag for ymldb operation
#define YMLDB_TAG_OP_GET "!get!"
#define YMLDB_TAG_OP_DELETE "!delete!"
#define YMLDB_TAG_OP_MERGE "!merge!"
#define YMLDB_TAG_OP_SUBSCRIBER "!subscriber!"
#define YMLDB_TAG_OP_PUBLISHER "!publisher!"
#define YMLDB_TAG_OP_SYNC "!sync!"
#define YMLDB_TAG_OP_SEQ "!seq!"

// yaml prefix for ymldb operation
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

// unix socket pathname
#define YMLDB_UNIXSOCK_PATH "@ymldb:%s"


// stream buffer API for ymldb
struct ymldb_stream *ymldb_stream_alloc(size_t len);
void ymldb_stream_close(struct ymldb_stream *buf);
FILE *ymldb_stream_open(struct ymldb_stream *buf, char *rw);
void ymldb_stream_free(struct ymldb_stream *buf);
struct ymldb_stream *ymldb_stream_alloc_and_open(size_t len, char *rw);

// create or delete ymldb
int ymldb_create(char *major_key, unsigned int flags);
void ymldb_destroy(char *major_key);
void ymldb_destroy_all();

// basic functions to update ymldb
int _ymldb_push(FILE *outstream, unsigned int opcode, char *major_key, char *format, ...);
int _ymldb_write(FILE *outstream, unsigned int opcode, char *major_key, ...);
char *_ymldb_read(char *major_key, ...);
struct ymldb_cb *_ymldb_cb(char *major_key);

// [YMLDB update facility - from string]
#define ymldb_read(major_key, ...) _ymldb_read(major_key, ##__VA_ARGS__, NULL)
#define ymldb_write(major_key, ...) _ymldb_write(NULL, YMLDB_OP_MERGE, major_key, ##__VA_ARGS__, NULL)
#define ymldb_delete(major_key, ...) _ymldb_write(NULL, YMLDB_OP_DELETE, major_key, ##__VA_ARGS__, NULL)
#define ymldb_sync(major_key, ...) _ymldb_write(NULL, YMLDB_OP_SYNC, major_key, ##__VA_ARGS__, NULL)
#define ymldb_get(outstream, major_key, ...) _ymldb_write(outstream, YMLDB_OP_GET, major_key, ##__VA_ARGS__, NULL)
// write ymldb using YAML document format.
int ymldb_push(char *major_key, char *format, ...);
// read ymldb using YAML document format.
int ymldb_pull(char *major_key, char *format, ...);

// [YMLDB update facility - from file]
// update ymldb using file descriptors.
int ymldb_run(char *major_key, int infd, int outfd);
// update ymldb using FILE* standard stream.
#define ymldb_run_with_stream(major_key, instream, outstream) _ymldb_run(_ymldb_cb(major_key), instream, outstream)

// [YMLDB distribution facility]
// enable ymldb distribution.
int ymldb_distribution_init(char *major_key, int flags);
// disable ymldb distribution.
int ymldb_distribution_deinit(char *major_key);
// add a file descripter as a subscriber. (The EOF from fd will terminate this connection.)
int ymldb_distribution_add(char *major_key, int subscriber_fd);
// add a file descripter as a subscriber.
int ymldb_distribution_delete(char *major_key, int subscriber_fd);
// set FD_SET of ymldb distribution.
int ymldb_distribution_set(fd_set *set);
// check FD_SET and receive the ymldb request and response from the remote.
int ymldb_distribution_recv(fd_set *set);

int _ymldb_callback_register(ymldb_callback_fn usr_func, void *usr_data, char *major_key, ...);
int _ymldb_callback_unregister(char *major_key, ...);

#define ymldb_callback_register(usr_func, usr_data, major_key, ...) \
    _ymldb_callback_register(usr_func, usr_data, major_key, ##__VA_ARGS__, NULL)
#define ymldb_callback_unregister(major_key, ...) \
    _ymldb_callback_unregister(major_key, ##__VA_ARGS__, NULL)

// [YMLDB data retrieval facility]
// print all ymldb data to the stream.
void ymldb_dump_all(FILE *outstream);

// print partical ymldb data.
#define ymldb_dump(outstream, major_key, ...) _ymldb_write(outstream, YMLDB_OP_GET, major_key, ##__VA_ARGS__, NULL)
#endif