#ifndef __YDB__
#define __YDB__

// **YAML DataBlock (YDB)** is a library to manage the hierarchical configuration 
// and statistical data simply and clearly using YAML input/output. 
// YDB internally builds the hierarchical data block from the serialized input stream 
// formed as YAML. And it supports the facilities to search, inquiry and 
// iterate each internal data in YDB using the API.

#include <stdio.h>
#include <ylist.h>

#define YDB_LEVEL_MAX 16
#define YDB_CONN_MAX 16
#define YDB_DEFAULT_TIMEOUT 3000 //ms
#define YDB_DELIVERY_LATENCY 100 //ms
#define YDB_DEFAULT_PORT 3677

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum _ydb_res
{
    YDB_OK = 0,
    YDB_NO_ERR = YDB_OK,
    YDB_W_TIMEOUT,
    YDB_W_MORE_RECV,
    YDB_W_NON_EXISTENT_DATA,
    YDB_W_DISCONN,
    YDB_WARNING_MIN = YDB_W_TIMEOUT,
    YDB_WARNING_MAX = YDB_W_DISCONN,

    YDB_ERROR,
    YDB_E_TIMER,
    YDB_E_EVENT,
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
    YDB_E_INVALID_MSG,
    YDB_E_ENTRY_EXISTS,
    YDB_E_NO_CONN,
    YDB_E_CONN_FAILED,
    YDB_E_CONN_CLOSED,
    YDB_E_FUNC,
    YDB_E_HOOK_ADD,
    YDB_E_UNKNOWN_TARGET,
    YDB_E_DENIED_DELETE,
} ydb_res;

#define YDB_SUCCESS(res) ((res) == YDB_OK)
#define YDB_WARNING(res) ((res) >= YDB_WARNING_MIN && (res) <= YDB_WARNING_MAX)
#define YDB_FAILED(res) ((res) >= YDB_ERROR)

char *ydb_res_str(ydb_res res);
void ydb_connection_log(int enable);

// YAML DataBlock structure
typedef struct _ydb ydb;
typedef struct _ynode ynode; // The YAML node of YDB (YAML DataBlock)

// str2yaml --
// Return new string converted to YAML character set.
// It should be free
char *str2yaml(char *cstr);

// str2yaml --
// Return new C string converted from YAML character set.
char *yaml2str(char *ystr, size_t len);

// binary_to_base64 --
// Return base64 string with the length.
// It should be free
char *binary_to_base64(unsigned char *binary, size_t binarylen, size_t *base64len);

// binary_to_base64_if --
// Return base64 string with LF (Line Feed).
// It should be free
char *binary_to_base64_lf(unsigned char *binary, size_t binarylen, size_t *base64len);

// base64_to_binary --
// Return base64 string with the length.
// It should be free
unsigned char *base64_to_binary(char *base64, size_t base64len, size_t *binarylen);

// ydb_lock --
// Lock the entrace of the YDB instance
void ydb_lock(struct _ydb *datablock);

// ydb_unlock --
// Unlock the entrace of the YDB instance
void ydb_unlock(struct _ydb *datablock);

// ydb_open --
// Open an instance of YAML DataBlock
ydb *ydb_open(char *name);

// ydb_get --
// Get the opend YAML DataBlock and also return ynode
ydb *ydb_get(char *name_and_path, ynode **node);

// ydb_name_and_path --
// Returns the name_and_path consisting of the YDB name and the path to the node.
// The name_and_path (/YDBNAME/path/to/node) must be free().
char *ydb_name_and_path(ynode *node, int *pathlen);

// ydb_name --
// Get the name of the YAML DataBlock
const char *ydb_name(ydb *datablock);

// ydb_connect --
// Create or connect to YDB IPC (Inter Process Communication) channel
//  - address: YDB communication channel address.
//   - us://unix-socket-name (unix socket)
//   - uss://unix-socket-name (hidden unix socket; socket file doesn’t appear from filesystem.)
//   - tcp://ipaddr:port (tcp)
//   - fifo://named-fifo-input,named-fifo-output
//  - flags:
//    pub(publisher)/sub(subscriber): YDB role configuration
//    w(writable): connect to the channel to write data in subscriber role.
//    u(unsubscribe): disable the subscription of the data change
//    s(sync-before-read mode): request the update of the YDB instance before ydb_read()
// e.g. ydb_connect(db, "uss://netconf", "pub")
//      ydb_connect(db, "us:///tmp/ydb_channel", "sub")
ydb_res ydb_connect(ydb *datablock, char *addr, char *flags);

// ydb_disconnect --
// Destroy or disconnect to the YDB IPC (Inter Process Communication) channel
ydb_res ydb_disconnect(ydb *datablock, char *addr);

// ydb_is_connected --
// Check the YDB IPC channel connected or not.
int ydb_is_connected(ydb *datablock, char *addr);

// ydb_is_server()
// Check the YDB IPC channel is running as server
int ydb_is_server(ydb *datablock, char *addr);

// ydb_is_publisher()
// Check the YDB IPC channel is running as server
int ydb_is_publisher(ydb *datablock, char *addr);

// ydb_serve --
// Run ydb_serve() in the main loop if YDB IPC channel is used.
// ydb_serve() updates the local YDB instance using the received YAML data from remotes.
ydb_res ydb_serve(ydb *datablock, int timeout);

// ydb_fd --
// Return the fd (file descriptor) opened for YDB IPC channel.
int ydb_fd(ydb *datablock);

// ydb_clear --
// Clear all data in the YAML DataBlock
ydb_res ydb_clear(ydb *datablock);

// ydb_close --
// Close the instance of YAML DataBlock
void ydb_close(ydb *datablock);

// return the path of the node. (the path must be free.)
char *ydb_path(ydb *datablock, ynode *node, int *pathlen);
// return the path of the node. (the path must be free.)
char *ydb_path_and_value(ydb *datablock, ynode *node, int *pathlen);

// ydb_search --
// return the node in the path (/path/to/data).
ynode *ydb_search(ydb *datablock, const char *format, ...);

// return the path between ancestor and descendant. (the path must be free.)
char *ydb_path_nodes(ynode *ancestor, ynode *descendant, int *pathlen);

// return the ylist instance that tokenizes the path.
// ylist and each entry should be free (ylist_destroy_custom(ylist, free)).
ylist *ydb_path_tokenize(char *path, char **val);

// return the top node of the yaml data block.
ynode *ydb_top(ydb *datablock);
// return the root node of the yaml data block.
ynode *ydb_root(ydb *datablock);
// return 1 if the node has no child.
int ydb_empty(ynode *node);
// return the number of child nodes.
int ydb_size(ynode *node);

// return the found child by the key.
ynode *ydb_find_child(ynode *base, char *key);
// Return the found node by the path
ynode *ydb_find(ynode *base, const char *format, ...);
// Return the found child by prefix match.
ynode *ydb_find_child_by_prefix(ynode *base, char *prefix);

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
// return node tag
const char *ydb_tag(ynode *node);
// Return node value if that is a value node.
const char *ydb_value(ynode *node);
// Return the key of the node when the parent is a map (hasp).
const char *ydb_key(ynode *node);
// Return the index of the node when the parent is a seq (list).
int ydb_index(ynode *node);
// Return the level of two nodes.
int ydb_level(ynode *top, ynode *node);

// return YAML string for node.
char *ydb_ynode2yaml(ydb *datablock, ynode *node, int *slen);

// ydb_retrieve --
// read the data from the current node (n)
int ydb_retrieve(ynode *n, const char *format, ...);

// ydb_clean --
// Remove all child nodes.
ydb_res ydb_clean(ydb *datablock, ynode *n);

// ydb_pase --
// Update the data into the ydb using file stream.
ydb_res ydb_parse(ydb *datablock, FILE *stream);

// ydb_pase --
// Update the data into the ydb from a buffer
ydb_res ydb_parses(ydb *datablock, char *buf, size_t buflen);

// ydb_dump --
// Print the data in the ydb into a file stream.
int ydb_dump(ydb *datablock, FILE *stream);

// ydb_dumps --
// Print the data in the ydb into a buffer.
int ydb_dumps(ydb *datablock, char **buf, size_t *buflen);

// ydb_dump_debug --
// Print the data into a file stream for debugging.
int ydb_dump_debug(ydb *datablock, FILE *stream);

// ydb_add --
// Add data to YDB using YAML input string 
// (that should be terminated with null as a string)
ydb_res ydb_add(ydb *datablock, char *string);

// ydb_rm --
// Delete data from YDB using YAML input string 
// (that shod be terminated with null as a string)
ydb_res ydb_rm(ydb *datablock, char *string);

// ydb_write --
// Update YDB using YAML input string
ydb_res ydb_write(ydb *datablock, const char *format, ...);

// ydb_delete --
// Delete data from YDB using YAML input string
ydb_res ydb_delete(ydb *datablock, const char *format, ...);

// ydb_read --
// Read the date from ydb such as the scanf() (YAML format)
// ydb_read() only fills the scalar value of the YAML mapping or list nodes.
// And it returns the number of found values.
//  - ydb_read(datablock, "key: %s\n"); // ok.
//  - ydb_read(datablock, "%s: value\n"); // not allowed.
int ydb_read(ydb *datablock, const char *format, ...);

// ydb_fprintf --
// Print the target data nodes to the stream
int ydb_fprintf(FILE *stream, ydb *datablock, const char *format, ...);

// ydb_path_write --
// Update & delete the ydb using input path and value
// ydb_path_write(datablock, "/path/to/update=%d", value)
ydb_res ydb_path_write(ydb *datablock, const char *format, ...);

// ydb_path_delete --
// Delete the ydb using input path
// ydb_path_delete(datablock, "/path/to/update\n")
ydb_res ydb_path_delete(ydb *datablock, const char *format, ...);

// ydb_path_read --
// Read the value from ydb using input path
// const char *value = ydb_path_read(datablock, "/path/to/read")
const char *ydb_path_read(ydb *datablock, const char *format, ...);

int ydb_path_fprintf(FILE *stream, ydb *datablock, const char *format, ...);

// ydb_read_hook: The callback executed by ydb_read() to update the datablock at reading.
//  - ydb_read_hook0 - 4: The callback prototype according to the USER (U1-4) number.
//  - path: The target path to be updated
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

// ydb_write_hook: The callback is executed by ydb_write() or ydb_delete().
//  - ydb_write_hook0 - 4: The callback prototype according to the USER-defined data (U1-4) number.
//  - op: 0: none, c: create, d: delete, r: replace
//  - _base: The base data node of ydb_write_hook registered
//  - _cur: The current data node (old data)
//  - _new: The new data node to be replaced or created.
//  - path: The path of ydb_read_hook registered
//  - U1-4: The USER-defined data
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

// ydb_onchange_hook: executed before, after ydb data changes
typedef void (*ydb_onchange_hook)(ydb *datablock, int started, void *user);
ydb_res ydb_onchange_hook_add(ydb *datablock, ydb_onchange_hook hook, void *user);
void ydb_onchange_hook_delete(ydb *datablock);

// update and delete the remote ydb targeted by origin.
ydb_res ydb_whisper_merge(ydb *datablock, char *path, const char *format, ...);
ydb_res ydb_whisper_delete(ydb *datablock, char *path, const char *format, ...);

// set the timeout of the YDB synchonization
ydb_res ydb_timeout(ydb *datablock, int msec);

// synchornize the remote ydb manually.
ydb_res ydb_sync(ydb *datablock, const char *format, ...);

// synchornize the remote ydb manually.
ydb_res ydb_path_sync(ydb *datablock, const char *format, ...);

// Traverse all child branches and leaves of a node.
//  - datablock: The datablock to traverse.
//  - cur: The current node to be traversed
//  - cb: The callback function invoked on each child node
//  - flags: The options to configure the traversing rule.
//    - leaf-only: The cb is invoked only if the traversing node is a leaf node.
//    - val-only: The cb is invoked only if the traversing node is a value node.
//    - leaf-first: Leaf nodes are traversed prior to branches.
//    - NULL: no-flags (traverse all branches and leaves.)
//  - num: The number of the user-defined data
//  - U1-4: The user-defined data
typedef ydb_res (*ydb_traverse_callback0)(ydb *datablock, ynode *cur);
typedef ydb_res (*ydb_traverse_callback1)(ydb *datablock, ynode *cur, void *U1);
typedef ydb_res (*ydb_traverse_callback2)(ydb *datablock, ynode *cur, void *U1, void *U2);
typedef ydb_res (*ydb_traverse_callback3)(ydb *datablock, ynode *cur, void *U1, void *U2, void *U3);
typedef ydb_res (*ydb_traverse_callback4)(ydb *datablock, ynode *cur, void *U1, void *U2, void *U3, void *U4);
typedef ydb_traverse_callback1 ydb_traverse_callback;
ydb_res ydb_traverse(ydb *datablock, ynode *cur, ydb_traverse_callback func, char *flags, int num, ...);

// disable variable arguments of the ydb instance for golang
void ydb_disable_variable_arguments(ydb *datablock);

#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#endif // __YDB__
