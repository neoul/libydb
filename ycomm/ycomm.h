#ifndef __YIPC__
#define __YIPC__

#include <cprops/avl.h>

// YIPC result
typedef enum yipc_res_type {
    YIPC_OK = 0,
    YIPC_ERR,
    YIPC_ERR_FILE_OPEN,
    YIPC_ERR_NO_ARG,
} yipc_res_t;

typedef enum yipc_ynode_type {
    YMLDB_LEAF,
    YMLDB_LIST,
    YMLDB_BRANCH // key exists
} yipc_ynode_t;


#define YIPC_CB_MAX 16

typedef struct yipc_cb_data_s
{
    char *keys[YIPC_CB_MAX];
    int keys_num;
    int keys_level;
    char *value;
    int resv:28;
    int deleted:1;
    int unregistered:1;
    int type:2;
} yipc_cb_data_t;

typedef void (*yipc_cb_func_t)(yipc_cb_data_t *meta, void *usr_data);

typedef struct yipc_cb_s
{
    struct ynode *ydb;
    yipc_cb_func_t func;
    yipc_cb_data_t *meta;
    void *usr_data;
    int resv : 29;
    int deleted : 1;
    int type : 2;
} yipc_cb_t;

typedef struct yipc_node_s
{
    char *key;
    int top:1;
    int level:31;
    yipc_ynode_t type;
    union {
        cp_avltree *children;
        char *value;
    };
    struct yipc_node_s *parent;
    yipc_cb_t *callback;
} yipc_node_t;


// yipc read data from a yaml file 
yipc_node_t *yipc_yaml_read_file(char *file);

#endif // __YIPC__
