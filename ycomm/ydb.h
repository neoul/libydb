#ifndef __YDB__
#define __YDB__

#define YDB_LEVEL_MAX 32

typedef enum _ydb_res
{
    YDB_OK = 0,
    YDB_ERR,
	YDB_E_NO_ARGS,
	YDB_E_TYPE_ERR,
    YDB_E_INVALID_PARENT,
    YDB_E_NO_ENTRY,
    YDB_E_DUMP_CB,
    YDB_E_MEM,
    YDB_E_FULL_BUF,
} ydb_res;





// low level function for ydb

// ynode type
#define YNODE_TYPE_VAL      0
#define YNODE_TYPE_DICT     1
#define YNODE_TYPE_LIST     2

typedef struct _ynode ynode;

// create ynode
ynode *ynode_new(unsigned char type, char *value);

// delete ynode regardless of the detachment of the parent
void ynode_free(ynode *node);

// insert the node to the parent, the key will be used for dict node.
ydb_res ynode_attach(ynode *node, ynode *parent, char *key);

// return parent after remove the node from the parent node.
ynode *ynode_detach(ynode *node);

// dump ydb
void ynode_dump_debug(ynode *node, unsigned int level);
void ynode_dump(ynode *node, unsigned int level);

int ynode_snprintf(char *buf, int buflen, ynode *node, int level);

#endif // __YDB__
