#ifndef __YDB__
#define __YDB__

// YAML DataBlock for Configuration Data Management 
// using YAML and IPC (Inter Process Communication)

#include <stdio.h>

#define YDB_LEVEL_MAX 32
#define YDB_CONN_MAX 32

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
    } ydb_res;

    extern char *ydb_res_str[];

#define YDB_LOG_DBG 5
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

    // Get YAML DataBlock
    ydb *ydb_get(char *name, ydb_iter **iter);

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

    // return the node in the path of the yaml data block.
    ydb_iter *ydb_search(ydb *datablock, char *path);
    // return the top node of the yaml data block.
    ydb_iter *ydb_top(ydb *datablock);

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
    unsigned char ydb_type(ydb_iter *iter);
    // return node value if that is a leaf.
    char *ydb_value(ydb_iter *iter);
    // return node key if that has a hash key.
    char *ydb_key(ydb_iter *iter);
    // return node index if the nodes' parent is a list.
    int ydb_index(ydb_iter *iter);

    // update the data in the ydb using file stream
    ydb_res ydb_parse(ydb *datablock, FILE *fp);

    // print the data in the ydb into the file stream
    int ydb_dump(ydb *datablock, FILE *fp);
    int ydb_dumps(ydb *datablock, char **buf, size_t *buflen);

    // update and delete data in ydb using the input string (yaml format)
    ydb_res ydb_write(ydb *datablock, const char *format, ...);
    ydb_res ydb_delete(ydb *datablock, const char *format, ...);

    // read the date from ydb as the scanf() (yaml format)
    int ydb_read(ydb *datablock, const char *format, ...);

    // ydb_update_hook is a callback function executed by ydb_read() to update ydb at reading.
    typedef ydb_res (*ydb_update_hook)(FILE *fp, ydb_iter *target, void *user);
    ydb_res ydb_update_hook_add(ydb *datablock, char *path, ydb_update_hook hook, void *user);
    void *ydb_update_hook_delete(ydb *datablock, char *path);

    // update & delete the ydb using input path and value
    // ydb_path_write(datablock, "/path/to/update=%d", value)
    ydb_res ydb_path_write(ydb *datablock, const char *format, ...);
    ydb_res ydb_path_delete(ydb *datablock, const char *format, ...);

    // read the value from ydb using input path
    // char *value = ydb_path_read(datablock, "/path/to/update")
    char *ydb_path_read(ydb *datablock, const char *format, ...);

    ydb_res ydb_serve(ydb *datablock, int timeout);

    int ydb_fd(ydb *datablock);

#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#endif // __YDB__
