#ifndef __YNODE__
#define __YNODE__

#ifdef __cplusplus
extern "C" {
#endif

// ynode type
#define YNODE_TYPE_VAL      0
#define YNODE_TYPE_DICT     1
#define YNODE_TYPE_LIST     2
#define YNODE_TYPE_MAX      3


typedef struct _ynode ynode;
typedef struct _ynode_record ynode_record;
ynode_record *ynode_record_new(FILE *fp, int fd, char *buf, int buflen, int start_level, int end_level);
void ynode_record_free(ynode_record *cb);
void ynode_record_indent_up(struct _ynode_record *record);
void ynode_record_indent_down(struct _ynode_record *record);

// ynode operation (Create, Merge, Delete) interfaces
// create single ynode and attach to parent.
// return created ynode.
ynode *ynode_create(unsigned char type, char *key, char *value, ynode *parent, ynode_record *record);

// create new ynodes to parent using src.
// return created ynode top.
ynode *ynode_create_copy(ynode *src, ynode *parent, char *key, ynode_record *record);

// create new ynodes using path.
// return the last created ynode.
ynode *ynode_create_path(char *path, ynode *parent, ynode_record *record);

// copy src ynodes (including all sub ynodes).
ynode *ynode_copy(ynode *src);

// merge src ynode to dest.
// dest is modified by the operation.
// return modified dest.
ynode *ynode_merge(ynode *dest, ynode *src, ynode_record *record);

// merge src ynode to dest.
// dest and src is not modified.
// New ynode is returned.
ynode *ynode_merge_new(ynode *dest, ynode *src);

// deleted cur ynode (including all sub ynodes).
void ynode_delete(ynode *node, ynode_record *record);

// dump ydb
void ynode_dump_node(FILE *fp, int fd, char *buf, int buflen, ynode *node, int start_level, int end_level);
#define ynode_dump_to_fp(fp, node, start_level, end_level) ynode_dump_node(fp, 0, NULL, 0, node, start_level, end_level)
#define ynode_dump_to_buf(buf, buflen, node, start_level, end_level) ynode_dump_node(NULL, 0, buf, buflen, node, start_level, end_level)
#define ynode_dump_to_fd(fd, node, start_level, end_level) ynode_dump_node(NULL, fd, NULL, 0, node, start_level, end_level)
#define ynode_dump(node, start_level, end_level) ynode_dump_node(stdout, 0, NULL, 0, node, start_level, end_level)

// write ynode db to buffer, fp, fd and stdout
int ynode_printf_to_buf(char *buf, int buflen, ynode *node, int start_level, int end_level);
int ynode_printf_to_fp(FILE *fp, ynode *node, int start_level, int end_level);
int ynode_printf_to_fd(int fd, ynode *node, int start_level, int end_level);
int ynode_printf(ynode *node, int start_level, int end_level);

// read ynode db from buffer, fp, fd and stdin
ydb_res ynode_scanf_from_fp(FILE *fp, ynode **n);
ydb_res ynode_scanf_from_fd(int fd, ynode **n);
ydb_res ynode_scanf_from_buf(char *buf, int buflen, ynode **n);
ydb_res ynode_scanf(ynode **n);

// detach and free ynode
void ynode_remove(ynode *n);

// high level interfaces to manage the ynode grapes
// update or create ynode n using the input string
ydb_res ynode_write(ynode **n, const char *format, ...);
// delete sub ynodes using the input string
ydb_res ynode_erase(ynode **n, const char *format, ...);
// read the date from ynode grapes as the scanf()
int ynode_read(ynode *n, const char *format, ...);


// [ynode searching & traveling]

// lookup an ynode in the path
ynode *ynode_search(ynode *node, char *path);

// find the ref ynode on the same position in target ynode grapes.
ynode *ynode_lookup(ynode *target, ynode *ref);

// return ynodes' type
unsigned char ynode_type(ynode *node);
// return ynodes' value if that is a leaf.
char *ynode_value(ynode *node);
// return ynodes' key if that has a hash key. 
char *ynode_key(ynode *node);
// return ynodes' index if the nodes' parent is a list.
int ynode_index(ynode *node);
// return the top node of the ynode. 
ynode *ynode_top(ynode *node);
// return the parent node of the ynode. 
ynode *ynode_up(ynode *node);
// return the first child node of the ynode. 
ynode *ynode_down(ynode *node);
// return the previous sibling node of the ynode. 
ynode *ynode_prev(ynode *node);
// return the next sibling node of the ynode. 
ynode *ynode_next(ynode *node);
// return the first sibling node of the ynode. 
ynode *ynode_first(ynode *node);
// return the last sibling node of the ynode. 
ynode *ynode_last(ynode *node);

// create a new path string for the ydb
// the level is the number of the parent and ancestors to be printed.
// the path returned should be free.
char *ynode_path(ynode *node, int level);

// create a new path and value string for the ydb
char *ynode_path_and_val(ynode *node, int level);

// ynode callback for hooking some change in ynode db.
typedef enum _yhook_op_type {
    YHOOK_OP_NONE,
    YHOOK_OP_CREATE,
    YHOOK_OP_REPLACE,
    YHOOK_OP_DELETE,
} yhook_op_type;
extern char *yhook_op_str[];

// flags for ynode hook and traverse func
#define YNODE_NO_FLAG 0x0
#define YNODE_LEAF_FIRST 0x1
#define YNODE_LEAF_ONLY 0x2

typedef void (*yhook_func)(yhook_op_type op, ynode *cur, ynode *_new, void *user);

// register the hook func to the target ynode.
ydb_res yhook_register(ynode *node, unsigned int flags, yhook_func func, void *user);

// unregister the hook func from the target ynode.
void yhook_unregister(ynode *node);


typedef ydb_res(*ynode_callback)(ynode *cur, void *addition);

ydb_res ynode_traverse(ynode *cur, ynode_callback cb, void *addition, unsigned int flags);

#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#endif // __YNODE__
