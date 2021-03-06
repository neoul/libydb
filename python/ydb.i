/* YDB SWIG for python */
%module ydb

%{
#define SWIG_FILE_WITH_INIT
#include "ydbcpp.h"
%}

// **YAML DataBlock (YDB)** is a library to manage the hierarchical configuration 
// and statistical data simply and clearly using YAML input/output. 
// YDB internally builds the hierarchical data block from the serialized input stream 
// formed as YAML. And it supports the facilities to search, inquiry and 
// iterate each internal data in YDB using the API.

typedef enum _ydb_res
{
    YDB_OK = 0,
    YDB_W_TIMEOUT,
    YDB_W_MORE_RECV,
    YDB_W_DISCONN,
    YDB_WARNING_MIN = YDB_W_TIMEOUT,
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
    YDB_E_DENIED_DELETE,
} ydb_res;

#define YDB_SUCCESS(res) ((res) == YDB_OK)
#define YDB_WARNING(res) ((res) >= YDB_WARNING_MIN && (res) <= YDB_WARNING_MAX)
#define YDB_FAILED(res) ((res) >= YDB_ERROR)

char *ydb_res_str(ydb_res res);
void ydb_connection_log(int enable);

// str2yaml --
// Return new string converted to YAML character set.
// It should be free
char *str2yaml(char *cstr);


// Ydb class --
// wrapped class of YDB

%newobject Ydb::get(char *filter);
%newobject Ydb::get();
%newobject Ydb::to_string();
%newobject Ydb::path(ynode *node);
%newobject Ydb::path_and_value(ynode *node);
%newobject Ydb::path_list(int depth, char *path);

class Ydb
{

public:
    Ydb(char *name);
    ~Ydb();

    // write --
    // write the YAML data
    ydb_res write(char *yaml);

    // remove --
    // delete the YAML data
    ydb_res remove(char *yaml);

    // The return value of get() must be free.
    char *get(char *filter);
    char *get();

    // path_write --
    // write the data using /path/to/data=value
    ydb_res path_write(char *path);

    // path_remove --
    // delete the data using /path/to/data
    ydb_res path_remove(char *path);

    // path_get --
    // get the data (value only) using /path/to/data
    const char *path_get(char *path);

    ydb_res connect(char* addr, char* flags);
    ydb_res disconnect(char* addr);

    // ydb_is_connected --
    // Check the YDB IPC channel connected or not.
    int is_connected(char *addr);

    // fd --
    // Return the fd (file descriptor) opened for YDB IPC channel.
    int fd();

    // serve --
    // Run serve() in the main loop if YDB IPC channel is used.
    // serve() updates the local YDB instance using the received YAML data from remotes.
    ydb_res serve(int timeout);

    // return the path of the node. (the path must be free.)
    char *path(ynode *node);
    // return the path of the node. (the path must be free.)
    char *path_and_value(ynode *node);
    // return the node in the path (/path/to/data).
    ynode *search(char *path);
    // return the top node of the yaml data block.
    ynode *top();
    // return 1 if the node has no child.
    int empty(ynode *node);
    // Return the found node by the path
    ynode *find(ynode *base, char *path);
    // return the parent node of the node.
    ynode *up(ynode *node);
    // return the first child node of the node.
    ynode *down(ynode *node);
    // return the previous sibling node of the node.
    ynode *prev(ynode *node);
    // return the next sibling node of the node.
    ynode *next(ynode *node);
    // return the first sibling node of the node.
    ynode *first(ynode *node);
    // return the last sibling node of the node.
    ynode *last(ynode *node);
    // return node tag
    const char *tag(ynode *node);
    // Return node value if that is a value node.
    const char *value(ynode *node);
    // Return the key of the node when the parent is a map (hasp).
    const char *key(ynode *node);
    // Return the index of the node when the parent is a seq (list).
    int index(ynode *node);
    // Return the level of two nodes.
    int level(ynode *base, ynode *node);

    // Return the list of path/to/data
    char *path_list(int depth, char *path);
};