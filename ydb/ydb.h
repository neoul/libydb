#ifndef __YDB__
#define __YDB__
#define YDB_VERSION "1.0"

// YAML DataBlock for Configuration Data Management
// using YAML and IPC (Inter Process Communication)

#include <stdio.h>

#define YDB_LEVEL_MAX 16
#define YDB_CONN_MAX 16
#define YDB_TIMEOUT 3000 //ms

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum _ydb_res
{
    YDB_OK = 0,
    YDB_W_UPDATED,
    YDB_W_MORE_RECV,
    YDB_W_DISCONN,
    YDB_WARNING_MIN = YDB_W_UPDATED,
    YDB_WARNING_MAX = YDB_W_DISCONN,

    YDB_ERROR,
    YDB_E_CTRL,
    YDB_E_SYSTEM_FAILED,
    YDB_E_STREAM_FAILED,
    YDB_E_PERSISTENCY_ERR,
    YDB_E_INVALID_ARGS,
    YDB_E_TYPE_ERR,
    YDB_E_INVALID_PARENT,
    YDB_E_NO_ENTRY,
    YDB_E_MEM_ALLOC,
    YDB_E_FULL_BUF,
    YDB_E_INVALID_YAML_TOKEN,
    YDB_E_YAML_INIT_FAILED,
    YDB_E_YAML_PARSING_FAILED,
    YDB_E_MERGE_FAILED,
    YDB_E_DELETE_FAILED,
    YDB_E_INVALID_MSG,
    YDB_E_ENTRY_EXISTS,
    YDB_E_NO_CONN,
    YDB_E_CONN_FAILED,
    YDB_E_CONN_CLOSED,
    YDB_E_FUNC,
    YDB_E_HOOK_ADD,
    YDB_E_UNKNOWN_TARGET,
} ydb_res;

#define YDB_SUCCESS(res) ((res) == 0)
#define YDB_WARNING(res) ((res) >= YDB_WARNING_MIN && (res) <= YDB_WARNING_MAX)
#define YDB_FAILED(res) ((res) >= YDB_ERROR)

char *ydb_res_str(ydb_res res);
void ydb_connection_log(int enable);

// YAML DataBlock structure
typedef struct _ydb ydb;
typedef struct _ynode ynode; // The node of YDB

// Open YAML DataBlock
ydb *ydb_open(char *name);

// Get YAML DataBlock and also return ynode
ydb *ydb_get(char *name_and_path, ynode **node);

// return the new string consisting of the YDB name and the path to the node.
// the return string must be free.
char *ydb_name_and_path(ynode *node, int *pathlen);

// Get the name of the YAML DataBlock
const char *ydb_name(ydb *datablock);

// address: use the unix socket if null
//          us://unix-socket-name (unix socket)
//          uss://unix-socket-name (hidden unix socket)
// flags: p(publisher)/s(subscriber)
//        w(writable): connect to a remote to write.
//        u(unsubscribe): disable the subscription of the change
// e.g. ydb_connect(db, "us:///tmp/ydb_channel", "sub:w")
ydb_res ydb_connect(ydb *datablock, char *addr, char *flags);
ydb_res ydb_disconnect(ydb *datablock, char *addr);
ydb_res ydb_is_connected(ydb *datablock, char *addr);

// Clear all data in the YAML DataBlock
ydb_res ydb_clear(ydb *datablock);
// Close the YAML DataBlock
void ydb_close(ydb *datablock);

// return the path of the node. (the path must be free.)
char *ydb_path(ydb *datablock, ynode *node, int *pathlen);
// return the path of the node. (the path must be free.)
char *ydb_path_and_value(ydb *datablock, ynode *node, int *pathlen);
// return the node in the path of the yaml data block.
ynode *ydb_search(ydb *datablock, char *path);

// return the path between ancestor and descendant;
char *ydb_path_nodes(ynode *ancestor, ynode *descendant, int *pathlen);

// return the top node of the yaml data block.
ynode *ydb_top(ydb *datablock);
// return the root node of the yaml data block.
ynode *ydb_root(ydb *datablock);
// return 1 if the node has no child.
int ydb_empty(ynode *node);


// return the found child by the key.
ynode *ydb_find_child(ynode *base, char *key);
// return the parent node of the node.
ynode *ydb_up(ynode *node);
// return the first child node of the node.
ynode *ydb_down(ynode *node);
// return the previous sibling node of the node.
ynode *ydb_prev(ynode *node);
// return the next sibling node of the node.
ynode *ydb_next(ynode *node);
// return the first sibling node of the node.
ynode *ydb_first(ynode *node);
// return the last sibling node of the node.
ynode *ydb_last(ynode *node);
// return node type
int ydb_type(ynode *node);
// return node value if that is a leaf.
const char *ydb_value(ynode *node);
// return node key if that has a hash key.
const char *ydb_key(ynode *node);
// return node index if the nodes' parent is a list.
int ydb_index(ynode *node);

// read the data from the node
int ynode_read(ynode *n, const char *format, ...);

// update the data in the ydb using file stream
ydb_res ydb_parse(ydb *datablock, FILE *stream);
ydb_res ydb_parses(ydb *datablock, char *buf, size_t buflen);

// print the data in the ydb into the file stream
int ydb_dump(ydb *datablock, FILE *stream);
int ydb_dumps(ydb *datablock, char **buf, size_t *buflen);

int ydb_dump_debug(ydb *datablock, FILE *stream);

// update and delete data in ydb using the input string (yaml format)
ydb_res ydb_write(ydb *datablock, const char *format, ...);
ydb_res ydb_delete(ydb *datablock, const char *format, ...);

// read the date from ydb as the scanf() (yaml format)
int ydb_read(ydb *datablock, const char *format, ...);

// print the target data to the stream
int ydb_fprintf(FILE *stream, ydb *datablock, const char *format, ...);

// update & delete the ydb using input path and value
// ydb_path_write(datablock, "/path/to/update=%d", value)
ydb_res ydb_path_write(ydb *datablock, const char *format, ...);
ydb_res ydb_path_delete(ydb *datablock, const char *format, ...);

// read the value from ydb using input path
// const char *value = ydb_path_read(datablock, "/path/to/update")
const char *ydb_path_read(ydb *datablock, const char *format, ...);

int ydb_path_fprintf(FILE *stream, ydb *datablock, const char *format, ...);

ydb_res ydb_serve(ydb *datablock, int timeout);

int ydb_fd(ydb *datablock);

// ydb_read_hook: The callback executed by ydb_read() to update the datablock at reading.
//  - ydb_read_hook0 - 4: The callback prototype according to the USER (U1-4) number.
//  - path: The path of ydb_read_hook registered
//  - stream: The stream to write the data into the datablock.
//        YAML format stream should be written by the ydb_read_hook.
//  - U1-4: The user-defined data
//  - num: The number of the user-defined data (U1-4)

typedef ydb_res (*ydb_read_hook0)(ydb *datablock, const char *path, FILE *stream);
typedef ydb_res (*ydb_read_hook1)(ydb *datablock, const char *path, FILE *stream, void *U1);
typedef ydb_res (*ydb_read_hook2)(ydb *datablock, const char *path, FILE *stream, void *U1, void *U2);
typedef ydb_res (*ydb_read_hook3)(ydb *datablock, const char *path, FILE *stream, void *U1, void *U2, void *U3);
typedef ydb_res (*ydb_read_hook4)(ydb *datablock, const char *path, FILE *stream, void *U1, void *U2, void *U3, void *U4);
typedef ydb_read_hook1 ydb_read_hook;

ydb_res ydb_read_hook_add(ydb *datablock, char *path, ydb_read_hook hook, int num, ...);
void ydb_read_hook_delete(ydb *datablock, char *path);

// ydb_write_hook: The callback executed by ydb_write() or the remote ydb update to notify the change of the datablock.
//  - ydb_write_hook0 - 4: The callback prototype according to the USER (U1-4) number.
//  - op: 0: none, c: create, d: delete, r: replace
//  - _cur: The current data node
//  - _new: The new data node to be replaced or created.
//  - path: The path of ydb_read_hook registered
//  - U1-4: The user-defined data
//  - path: The path of ydb_write_hook registered
//  - num: The number of the user-defined data (U1-4)

typedef void (*ydb_write_hook0)(ydb *datablock, char op, ynode *_base, ynode *_cur, ynode *_new);
typedef void (*ydb_write_hook1)(ydb *datablock, char op, ynode *_base, ynode *_cur, ynode *_new, void *U1);
typedef void (*ydb_write_hook2)(ydb *datablock, char op, ynode *_base, ynode *_cur, ynode *_new, void *U1, void *U2);
typedef void (*ydb_write_hook3)(ydb *datablock, char op, ynode *_base, ynode *_cur, ynode *_new, void *U1, void *U2, void *U3);
typedef void (*ydb_write_hook4)(ydb *datablock, char op, ynode *_base, ynode *_cur, ynode *_new, void *U1, void *U2, void *U3, void *U4);
typedef void (*ydb_write_suppressed_hook0)(ydb *datablock, char op, ynode *_base);
typedef void (*ydb_write_suppressed_hook1)(ydb *datablock, char op, ynode *_base, void *U1);
typedef void (*ydb_write_suppressed_hook2)(ydb *datablock, char op, ynode *_base, void *U1, void *U2);
typedef void (*ydb_write_suppressed_hook3)(ydb *datablock, char op, ynode *_base, void *U1, void *U2, void *U3);
typedef void (*ydb_write_suppressed_hook4)(ydb *datablock, char op, ynode *_base, void *U1, void *U2, void *U3, void *U4);
typedef ydb_write_hook1 ydb_write_hook;

ydb_res ydb_write_hook_add(ydb *datablock, char *path, int suppressed, ydb_write_hook func, int num, ...);
void ydb_write_hook_delete(ydb *datablock, char *path);

// update and delete the remote ydb targeted by origin.
ydb_res ydb_whisper_merge(ydb *datablock, char *path, const char *format, ...);
ydb_res ydb_whisper_delete(ydb *datablock, char *path, const char *format, ...);

// synchornize the remote ydb manually.
ydb_res ydb_sync(ydb *datablock, const char *format, ...);
ydb_res ydb_path_sync(ydb *datablock, const char *format, ...);

// Travese all child branches and leaves of a node.
//  - datablock: The datablock to traverse.
//  - cur: The current node to be traversed
//  - cb: The callback function invoked on each child node
//  - flags: The options to configure the travesing rule.
//    - leaf-only: The cb is invoked only if the traversing node is a leaf node.
//    - val-only: The cb is invoked only if the traversing node is a value node.
//    - leaf-first: Leaf nodes are traversed prior to branches.
//    - NULL: no-flags (treverse all branches and leaves.)
//  - num: The number of the user-defined data
//  - U1-4: The user-defined data
typedef ydb_res (*ydb_traverse_callback0)(ydb *datablock, ynode *cur);
typedef ydb_res (*ydb_traverse_callback1)(ydb *datablock, ynode *cur, void *U1);
typedef ydb_res (*ydb_traverse_callback2)(ydb *datablock, ynode *cur, void *U1, void *U2);
typedef ydb_res (*ydb_traverse_callback3)(ydb *datablock, ynode *cur, void *U1, void *U2, void *U3);
typedef ydb_res (*ydb_traverse_callback4)(ydb *datablock, ynode *cur, void *U1, void *U2, void *U3, void *U4);
typedef ydb_traverse_callback1 ydb_traverse_callback;
ydb_res ydb_traverse(ydb *datablock, ynode *cur, ydb_traverse_callback func, char *flags, int num, ...);


#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#endif // __YDB__
