#ifndef __YMLDB__
#define __YMLDB__
#include <stdio.h>

#define YMLDB_LOG_LOG 3
#define YMLDB_LOG_INFO 2
#define YMLDB_LOG_ERR 1
#define YMLDB_LOG_NONE 0
int ymldb_log_set(int log_level, char *log_file);

// yaml tag for ymldb operation
#define YMLDB_TAG_OP_GET "!get!"
#define YMLDB_TAG_OP_DELETE "!delete!"
#define YMLDB_TAG_OP_MERGE "!merge!"
#define YMLDB_TAG_OP_SUBSCRIBER "!subscriber!"
#define YMLDB_TAG_OP_PUBLISHER "!publisher!"
#define YMLDB_TAG_OP_SYNC "!sync!"
#define YMLDB_TAG_OP_SEQ "!seq!"
#define YMLDB_TAG_OP_ACK "!ack!"

// yaml prefix for ymldb operation
#define YMLDB_TAG_BASE "ymldb:op:"
#define YMLDB_TAG_GET YMLDB_TAG_BASE "get"
#define YMLDB_TAG_DELETE YMLDB_TAG_BASE "delete"
#define YMLDB_TAG_MERGE YMLDB_TAG_BASE "merge"
#define YMLDB_TAG_SUBSCRIBER YMLDB_TAG_BASE "subscriber"
#define YMLDB_TAG_PUBLISHER YMLDB_TAG_BASE "publisher"
#define YMLDB_TAG_SYNC YMLDB_TAG_BASE "sync"
#define YMLDB_TAG_ACK YMLDB_TAG_BASE "ack"

#define YMLDB_TAG_SEQ_BASE "ymldb:seq:"
#define YMLDB_TAG_SEQ "ymldb:seq:e:" // the last message of this sequence
#define YMLDB_TAG_SEQ_CON "ymldb:seq:c:" // the consecutive messages exists

// opcode
#define YMLDB_OP_GET 0x01
#define YMLDB_OP_DELETE 0x02
#define YMLDB_OP_MERGE 0x04
#define YMLDB_OP_SEQ 0x08
#define YMLDB_OP_SUBSCRIBER 0x10
#define YMLDB_OP_PUBLISHER 0x20
#define YMLDB_OP_SYNC 0x40
#define YMLDB_OP_ACTION (YMLDB_OP_GET | YMLDB_OP_DELETE | YMLDB_OP_MERGE | YMLDB_OP_SYNC)
#define YMLDB_OP_SEQ_CON 0x80
#define YMLDB_OP_ACK 0x100

// flags
#define YMLDB_FLAG_NONE 0x00
#define YMLDB_FLAG_PUBLISHER 0x01 // publish ymldb if set, subscribe ymldb if not.
#define YMLDB_FLAG_SUBSCRIBER 0x02
#define YMLDB_FLAG_ASYNC 0x04
#define YMLDB_FLAG_RECONNECT 0x100
#define YMLDB_FLAG_INSYNC 0x200
#define YMLDB_FLAG_SUB_PUBLISHER 0x400
#define YMLDB_FLAG_NO_RECORD 0x800
#define YMLDB_FLAG_CONN (YMLDB_FLAG_PUBLISHER | YMLDB_FLAG_SUBSCRIBER | YMLDB_FLAG_SUB_PUBLISHER) // communcation channel enabled


// unix socket pathname
#define YMLDB_UNIXSOCK_PATH "@ymldb:%s"

#define YMLDB_STREAM_THRESHOLD 4094
#define YMLDB_STREAM_BUF_SIZE (YMLDB_STREAM_THRESHOLD + 128)

// create or delete ymldb
int ymldb_is_created(char *major_key);
int ymldb_create(char *major_key, unsigned int flags);
void ymldb_destroy(char *major_key);
void ymldb_destroy_all();

// basic functions to update ymldb
int _ymldb_write(FILE *outstream, unsigned int opcode, char *major_key, ...);
int _ymldb_write2(FILE *outstream, unsigned int opcode, int keys_num, char *keys[]);
char *_ymldb_read(char *major_key, ...);
char *_ymldb_read2(int keys_num, char *keys[]);

// [YMLDB update facility - from string]
#define ymldb_write(major_key, ...) _ymldb_write(NULL, YMLDB_OP_MERGE, major_key, ##__VA_ARGS__, NULL)
#define ymldb_delete(major_key, ...) _ymldb_write(NULL, YMLDB_OP_DELETE, major_key, ##__VA_ARGS__, NULL)
#define ymldb_sync(major_key, ...) _ymldb_write(NULL, YMLDB_OP_SYNC, major_key, ##__VA_ARGS__, NULL)
#define ymldb_sync_ack(major_key, ...) \
    _ymldb_write(NULL, (YMLDB_OP_SYNC | YMLDB_OP_ACK), major_key, ##__VA_ARGS__, NULL)

#define ymldb_get(OUTSTREAM, major_key, ...) _ymldb_write(OUTSTREAM, YMLDB_OP_GET, major_key, ##__VA_ARGS__, NULL)
#define ymldb_read(major_key, ...) _ymldb_read(major_key, ##__VA_ARGS__, NULL)

#define ymldb_write2(KEYS_NUM, KEYS) _ymldb_write2(NULL, YMLDB_OP_MERGE, KEYS_NUM, KEYS)
#define ymldb_delete2(KEYS_NUM, KEYS) _ymldb_write2(NULL, YMLDB_OP_DELETE, KEYS_NUM, KEYS)
#define ymldb_sync2(KEYS_NUM, KEYS) _ymldb_write2(NULL, YMLDB_OP_SYNC, KEYS_NUM, KEYS)
#define ymldb_get2(OUTSTREAM, KEYS_NUM, KEYS) _ymldb_write2(OUTSTREAM, YMLDB_OP_GET, KEYS_NUM, KEYS)
#define ymldb_read2(KEYS_NUM, KEYS) _ymldb_read2(KEYS_NUM, KEYS)

// write ymldb data using YAML document format.
int ymldb_push(char *major_key, char *format, ...);
// read ymldb data using YAML document format.
int ymldb_pull(char *major_key, char *format, ...);

// [YMLDB update facility - from file]
// update ymldb using file descriptors.
int ymldb_run_with_fd(char *major_key, int infd, int outfd);
int ymldb_run(char *major_key, FILE *instream, FILE *outstream);

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
int ymldb_distribution_recv_and_dump(FILE *outstream, fd_set *set);

// return new_fd (>0) if this cur_fd is server.
// return 0 if the reception is ok.
// return -1 if the reception is failed.
int ymldb_distribution_recv_fd(int *cur_fd);
int ymldb_distribution_recv_fd_and_dump(FILE *outstream, int *cur_fd);

// YMLDB callback type
#define YMLDB_UPDATE_CALLBACK 0x01
#define YMLDB_NOTIFY_CALLBACK 0x00

#define YMLDB_CALLBACK_MAX 16
struct ymldb_callback_data
{
    char *keys[YMLDB_CALLBACK_MAX];
    int keys_num;
    int keys_level;
    char *value;
    int resv:28;
    int deleted:1;
    int unregistered:1;
    int type:2;
};
typedef void (*ymldb_callback_fn)(void *usr_data, struct ymldb_callback_data *callback_data);

int _ymldb_callback_register(int type, ymldb_callback_fn usr_func, void *usr_data, char *major_key, ...);
int _ymldb_callback_unregister(char *major_key, ...);
int _ymldb_callback_register2(int type, ymldb_callback_fn usr_func, void *usr_data, int keys_num, char *keys[]);
int _ymldb_callback_unregister2(int keys_num, char *keys[]);

#define ymldb_callback_register(type, usr_func, usr_data, major_key, ...) \
    _ymldb_callback_register((type), (usr_func), (usr_data), (major_key), ##__VA_ARGS__, NULL)
#define ymldb_update_callback_register(usr_func, usr_data, major_key, ...) \
    _ymldb_callback_register(YMLDB_UPDATE_CALLBACK, (usr_func), (usr_data), (major_key), ##__VA_ARGS__, NULL)
#define ymldb_notify_callback_register(usr_func, usr_data, major_key, ...) \
    _ymldb_callback_register(YMLDB_NOTIFY_CALLBACK, (usr_func), (usr_data), (major_key), ##__VA_ARGS__, NULL)
#define ymldb_callback_unregister(major_key, ...) \
    _ymldb_callback_unregister(major_key, ##__VA_ARGS__, NULL)

#define ymldb_callback_register2(type, usr_func, usr_data, keys_num, keys) \
    _ymldb_callback_register2((type), (usr_func), (usr_data), (keys_num), (keys))
#define ymldb_update_callback_register2(usr_func, usr_data, keys_num, keys) \
    _ymldb_callback_register2((YMLDB_UPDATE_CALLBACK), (usr_func), (usr_data), (keys_num), (keys))
#define ymldb_notify_callback_register2(usr_func, usr_data, keys_num, keys) \
    _ymldb_callback_register2((YMLDB_NOTIFY_CALLBACK), (usr_func), (usr_data), (keys_num), (keys))
#define ymldb_callback_unregister2(keys_num, keys) \
    _ymldb_callback_unregister2((keys_num), (keys))

// [YMLDB traverse facility]
// This facility is used to traverse each node of the YMLDB tree.

struct ymldb_iterator
{
    void *ydb;
    void *cur;
};

// internal function for ymldb iterator creation.
struct ymldb_iterator *_ymldb_iterator_init(struct ymldb_iterator *iter, char *major_key, ...);
struct ymldb_iterator *_ymldb_iterator_init2(struct ymldb_iterator *iter, int keys_num, char *keys[]);

// allocate new ymldb iterator
#define ymldb_iterator_alloc(major_key, ...) _ymldb_iterator_init(NULL, major_key, ##__VA_ARGS__, NULL)
#define ymldb_iterator_alloc2(keys_num, keys) _ymldb_iterator_init2(NULL, (keys_num), (keys))

// initilize the ymldb iterator without allocation.
#define ymldb_iterator_init(iter, major_key, ...) _ymldb_iterator_init(iter, major_key, ##__VA_ARGS__, NULL)
#define ymldb_iterator_init2(iter, keys_num, keys) _ymldb_iterator_init2(iter, (keys_num), (keys))
// remove the ymldb iterator data without free.
void ymldb_iterator_deinit(struct ymldb_iterator *iter);
// free the ymldb iterator.
void ymldb_iterator_free(struct ymldb_iterator *iter);
// back to the base node (started iterator).
int ymldb_iterator_reset(struct ymldb_iterator *iter);
// change the base node (started iterator) to the current node.
int ymldb_iterator_rebase(struct ymldb_iterator *iter);
// copy the src iterator (must be freed.)
struct ymldb_iterator *ymldb_iterator_copy(struct ymldb_iterator *src);

// lookup a child node using the key and then move to that node.
const char *ymldb_iterator_lookup_down(struct ymldb_iterator *iter, char *key);
// lookup a sibling node using the key and then move to that node.
const char *ymldb_iterator_lookup(struct ymldb_iterator *iter, char *key);

// go to the first child node, return NULL if not exist.
const char *ymldb_iterator_down(struct ymldb_iterator *iter);
// go to the parent node, return NULL if not exist.
const char *ymldb_iterator_up(struct ymldb_iterator *iter);
// go to the next node (the nearest sibling), return NULL if not exist.
const char *ymldb_iterator_next(struct ymldb_iterator *iter);
// go to the previous node (the nearest sibling), return NULL if not exist.
const char *ymldb_iterator_prev(struct ymldb_iterator *iter);

// get the value of the ymldb iterator.
const char *ymldb_iterator_get_value(struct ymldb_iterator *iter);
// get the key of the ymldb iterator.
const char *ymldb_iterator_get_key(struct ymldb_iterator *iter);

// write ymldb format data to a file.
int ymldb_file_push(char *filename, char *format, ...);
// read ymldb format data to a file.
int ymldb_file_pull(char *filename, char *format, ...);

// [YMLDB data retrieval facility]
// print all ymldb to the stream if NULL.
void ymldb_dump_all(FILE *outstream, char *major_key);

#endif
