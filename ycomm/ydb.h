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
    YDB_E_INVALID_YAML,
    YDB_E_INVALID_YAML_TOP,
    YDB_E_INVALID_YAML_KEY,
    YDB_E_INVALID_YAML_ENTRY,
} ydb_res;

extern char *ydb_err_str[];
#define YDB_LOG_DBG     3
#define YDB_LOG_INFO    2
#define YDB_LOG_ERR     1
#define YDB_LOG_CRI     0

// set the ydb log severity
extern unsigned int ydb_log_severity;
typedef int (*ydb_log_func)(int severity, const char *func, int line, const char *format, ...);
int ydb_log_register( ydb_log_func func);

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
void ynode_dump_node(FILE *fp, int fd, char *buf, int buflen, ynode *node, int level);
#define ynode_fp_dump(fp, node, level) ynode_dump_node(fp, 0, NULL, 0, node, level);
#define ynode_sn_dump(buf, buflen, node, level) ynode_dump_node(NULL, 0, buf, buflen, node, level);
#define ynode_fd_dump(fd, node, level) ynode_dump_node(NULL, fd, NULL, 0, node, level);
#define ynode_dump(node, level) ynode_dump_node(stdout, 0, NULL, 0, node, level);

int ynode_snprintf(char *buf, int buflen, ynode *node, int level);
int ynode_fprintf(FILE *fp, ynode *node, int level);
int ynode_write(int fd, ynode *node, int level);
int ynode_printf(ynode *node, int level);

ynode *ynode_fscanf(FILE *fp);
ynode *ynode_scanf();
ynode *ynode_read(int fd);

#endif // __YDB__
