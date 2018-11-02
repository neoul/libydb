#ifndef __YDB__
#define __YDB__

// YAML DataBlock for Configuration Data Management
// using YAML and IPC (Inter Process Communication)

#include <stdio.h>

#define YDB_LEVEL_MAX 32
#define YDB_CONN_MAX 32
#define YDB_TIMEOUT 3000 //ms

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum _ydb_res
    {
        YDB_OK = 0,
        YDB_ERROR,
        YDB_E_INVALID_ARGS,
        YDB_E_TYPE_ERR,
        YDB_E_INVALID_PARENT,
        YDB_E_NO_ENTRY,
        YDB_E_DUMP_CB,
        YDB_E_MEM,
        YDB_E_FULL_BUF,
        YDB_E_PERSISTENCY_ERR,
        YDB_E_INVALID_YAML_INPUT,
        YDB_E_INVALID_YAML_TOP,
        YDB_E_INVALID_YAML_KEY,
        YDB_E_INVALID_YAML_ENTRY,
        YDB_E_YAML_INIT,
        YDB_E_YAML_EMPTY_TOKEN,
        YDB_E_MERGE_FAILED,
        YDB_E_DELETE_FAILED,
        YDB_E_SYSTEM_FAILED,
        YDB_E_CONN_FAILED,
        YDB_E_CONN_CLOSED,
        YDB_E_CONN_DENIED,
        YDB_E_INVALID_MSG,
        YDB_E_RECV_REQUIRED,
        YDB_E_INVALID_FLAGS,
        YDB_E_ENTRY_EXISTS,
        YDB_E_STREAM_FAILED,
        YDB_E_NO_CONN,
    } ydb_res;

    extern char *ydb_res_str[];

#define YDB_LOG_DEBUG 5
#define YDB_LOG_INOUT 4
#define YDB_LOG_INFO 3
#define YDB_LOG_WARN 2
#define YDB_LOG_ERR 1
#define YDB_LOG_CRI 0

    // Set YAML DataBlock log severity
    extern unsigned int ydb_log_severity;
    typedef int (*ydb_log_func)(int severity, const char *func, int line, const char *format, ...);
    int ydb_log_register(ydb_log_func func);

    // YAML DataBlock structure
    typedef struct _ydb ydb;
    typedef struct _ynode ydb_iter; // The node reference of YDB

    // Open YAML DataBlock
    ydb *ydb_open(char *name);

    // Get YAML DataBlock and also return ydb_iter
    ydb *ydb_get(char *name_and_path, ydb_iter **iter);

    // return the new string consisting of the YDB name and the path to the iter.
    // the return string must be free.
    char *ydb_name_and_path(ydb_iter *iter, int *pathlen);

    // Get the name of the YAML DataBlock
    char *ydb_name(ydb *datablock);

    // address: use the unix socket if null
    //          us://unix-socket-name (unix socket)
    //          uss://unix-socket-name (hidden unix socket)
    // flags: p(publisher)/s(subscriber)
    //        w(writable): connect to a remote to write.
    //        u(unsubscribe): disable the subscription of the change
    // e.g. ydb_connect(db, "us:///tmp/ydb_channel", "sub:w")
    ydb_res ydb_connect(ydb *datablock, char *addr, char *flags);
    ydb_res ydb_reconnect(ydb *datablock, char *addr, char *flags);
    ydb_res ydb_disconnect(ydb *datablock, char *addr);

    // Clear all data in the YAML DataBlock
    ydb_res ydb_clear(ydb *datablock);
    // Close the YAML DataBlock
    void ydb_close(ydb *datablock);

    // return the path of the node. (the path must be free.)
    char *ydb_path(ydb *datablock, ydb_iter *iter, int *pathlen);
    // return the path of the node. (the path must be free.)
    char *ydb_path_and_value(ydb *datablock, ydb_iter *iter, int *pathlen);
    // return the node in the path of the yaml data block.
    ydb_iter *ydb_search(ydb *datablock, char *path);

    // return the top node of the yaml data block.
    ydb_iter *ydb_top(ydb *datablock);
    // return the root node of the yaml data block.
    ydb_iter *ydb_root(ydb *datablock);
    // return 1 if the node has no child.
    int ydb_empty(ydb_iter *iter);

    // return the parent node of the node.
    ydb_iter *ydb_up(ydb_iter *iter);
    // return the first child node of the node.
    ydb_iter *ydb_down(ydb_iter *iter);
    // return the previous sibling node of the node.
    ydb_iter *ydb_prev(ydb_iter *iter);
    // return the next sibling node of the node.
    ydb_iter *ydb_next(ydb_iter *iter);
    // return the first sibling node of the node.
    ydb_iter *ydb_first(ydb_iter *iter);
    // return the last sibling node of the node.
    ydb_iter *ydb_last(ydb_iter *iter);
    // return node type
    int ydb_type(ydb_iter *iter);
    // return node value if that is a leaf.
    char *ydb_value(ydb_iter *iter);
    // return node key if that has a hash key.
    char *ydb_key(ydb_iter *iter);
    // return node index if the nodes' parent is a list.
    int ydb_index(ydb_iter *iter);

    // update the data in the ydb using file stream
    ydb_res ydb_parse(ydb *datablock, FILE *fp);
    ydb_res ydb_parses(ydb *datablock, char *buf, size_t buflen);

    // print the data in the ydb into the file stream
    int ydb_dump(ydb *datablock, FILE *fp);
    int ydb_dumps(ydb *datablock, char **buf, size_t *buflen);

    // update and delete data in ydb using the input string (yaml format)
    ydb_res ydb_write(ydb *datablock, const char *format, ...);
    ydb_res ydb_delete(ydb *datablock, const char *format, ...);

    // read the date from ydb as the scanf() (yaml format)
    int ydb_read(ydb *datablock, const char *format, ...);

    // update & delete the ydb using input path and value
    // ydb_path_write(datablock, "/path/to/update=%d", value)
    ydb_res ydb_path_write(ydb *datablock, const char *format, ...);
    ydb_res ydb_path_delete(ydb *datablock, const char *format, ...);

    // read the value from ydb using input path
    // char *value = ydb_path_read(datablock, "/path/to/update")
    char *ydb_path_read(ydb *datablock, const char *format, ...);

    ydb_res ydb_serve(ydb *datablock, int timeout);

    int ydb_fd(ydb *datablock);

    // ydb_read_hook: The callback function executed by ydb_read()
    //   to update ydb at reading.
    // ydb_fp: The stream buffer to be written to the ydb
    //   YAML format stream should be written by the ydb_read_hook.
    // path: The path of ydb_read_hook registered
    // U1~U4: The user data

    typedef ydb_res (*ydb_read_hook0)(ydb *db, char *path, FILE *ydb_fp);
    typedef ydb_res (*ydb_read_hook1)(ydb *db, char *path, FILE *ydb_fp, void *U1);
    typedef ydb_res (*ydb_read_hook2)(ydb *db, char *path, FILE *ydb_fp, void *U1, void *U2);
    typedef ydb_res (*ydb_read_hook3)(ydb *db, char *path, FILE *ydb_fp, void *U1, void *U2, void *U3);
    typedef ydb_res (*ydb_read_hook4)(ydb *db, char *path, FILE *ydb_fp, void *U1, void *U2, void *U3, void *U4);
    typedef ydb_read_hook1 ydb_read_hook;

    // ydb_read_hook_add/ydb_read_hook_delete:
    //   Register/unregister the callback function to the target ynode.
    // path: the path to the target ynode
    // hook: user-defined callback function called in ydb_read().
    // user: user's data
    // ydb_read_hook_delete: It returns the user's data registered.
    ydb_res ydb_read_hook_add(ydb *datablock, char *path, ydb_read_hook hook, int num, ...);
    void ydb_read_hook_delete(ydb *datablock, char *path);

    typedef void (*ydb_write_hook)(char op, ydb_iter *cur, ydb_iter *_new, void *user);

    // // flags: leaf-only, val-only,
    ydb_res ydb_write_hook_add(ydb *datablock, char *path, ydb_write_hook func, char *flags, void *user);
    void ydb_write_hook_delete(ydb *datablock, char *path);

#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#endif // __YDB__
