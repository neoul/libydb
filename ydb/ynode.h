#ifndef __YNODE__
#define __YNODE__

#include <ydb.h>

#ifdef __cplusplus
extern "C"
{
#endif

// ynode type
typedef enum
{
    YNODE_TYPE_NONE,
    YNODE_TYPE_VAL,
    YNODE_TYPE_LIST,
    YNODE_TYPE_MAP,
    YNODE_TYPE_SET, // a map only has keys.
    YNODE_TYPE_IMAP, // integer key map
    YNODE_TYPE_OMAP,
    YNODE_TYPE_MAX = YNODE_TYPE_OMAP,
} node_type;

typedef struct _ynode ynode;
typedef struct _ynode_log ynode_log;
ynode_log *ynode_log_open(ynode *top, FILE *dumpfp);
void ynode_log_close(ynode_log *log, char **buf, size_t *buflen);

// get the src nodes' data using the log (ynode_log).
// return the number of value nodes printed to the log (ynode_log).
int ynode_get(ynode *src, ynode_log *log);

// ynode operation (Create, Merge, Delete) interfaces
// create single ynode and attach to parent.
// return created ynode.
ynode *ynode_create(node_type type, const char *tag, const char *key, char *value, ynode *parent, ynode_log *log);

// create new ynodes to parent using src.
// return created ynode top.
ynode *ynode_create_copy(ynode *src, ynode *parent, const char *key, ynode_log *log);

// create new ynodes using path.
// return the last created ynode.
ynode *ynode_create_path(char *path, ynode *parent, ynode_log *log);

// copy src ynodes (including all sub ynodes).
ynode *ynode_copy(ynode *src);

// merge src ynode to dest.
// dest is modified by the operation.
// return modified dest.
ynode *ynode_merge(ynode *dest, ynode *src, ynode_log *log);

// merge src ynode to dest.
// dest and src is not modified.
// New ynode is returned.
ynode *ynode_merge_new(ynode *dest, ynode *src);

// deleted cur ynode (including all sub ynodes).
void ynode_delete(ynode *node, ynode_log *log);

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

// print ynode meta data.
int ynode_fprintf_meta(FILE *fp, ynode *node);

// read ynode db from buffer, fp, fd and stdin
ydb_res ynode_scanf_from_fp(FILE *fp, ynode **n);
ydb_res ynode_scanf(ynode **n);
ydb_res ynode_scanf_from_fd(int fd, ynode **n);
ydb_res ynode_scanf_from_buf(char *buf, int buflen, int origin, ynode **n);

// detach and free ynode
void ynode_remove(ynode *n);

// high level interfaces to manage the ynode grapes
// update or create ynode n using the input string
ydb_res ynode_write(ynode **n, const char *format, ...);
// delete sub ynodes using the input string
ydb_res ynode_erase(ynode **n, const char *format, ...);
// read the date from ynode grapes as the scanf()
int ydb_retrieve(ynode *n, const char *format, ...);

// [ynode searching & traveling]

// lookup an ynode in the path
ynode *ynode_search_best(ynode *base, char *path, int *matched);
ynode *ynode_search(ynode *base, char *path);

// find the ref ynode in target ynode tree.
// if ignore_index set, the first entry of the list are selected.
ynode *ynode_lookup(ynode *target, ynode *ref, int ignore_index);

// return ynodes' type
int ynode_type(ynode *node);

// return ynodes' tag
const char *ynode_tag(ynode *node);

// return the ynode has a value or children
int ynode_empty(ynode *node);
// return the number of chilren
int ynode_size(ynode *node);

// return ynodes' value if that is a leaf.
const char *ynode_value(ynode *node);
// return ynodes' key if that has a hash key.
const char *ynode_key(ynode *node);
// return ynodes' index if the nodes' parent is a list.
int ynode_index(ynode *node);
// return ynodes' origin
int ynode_origin(ynode *node);

// return the found child by the key.
ynode *ynode_find_child(ynode *node, const char *key);
// find a nearby child by the key.
ynode *ynode_find_nearby(ynode *node, const char *key, int lower);

// return ylist from the path tokenized.
ylist *ynode_path_tokenize(char *path, char **val);

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

// Return the level from top to node.
int ynode_level(ynode *top, ynode *node);

// create a new path string for the ydb
// the level is the number of the parent and ancestors to be printed.
// the path returned must be free.
char *ynode_path(ynode *node, int level, int *pathlen);

// create a new path string with prefix and postfix strings.
// <prefix>/<node_path>/<postfix>
// the level is the number of the parent and ancestors to be printed.
// the path returned must be free.
char *ynode_path_with_pre_postfix(ynode *node, int level, int *pathlen, char *prefix, char *postfix);

// create a new path and value string for the ydb
char *ynode_path_and_val(ynode *node, int level, int *pathlen);

// ynode callback for hooking some change in ynode db.
#define YHOOK_OP_NONE 0x0
#define YHOOK_OP_CREATE 'c'
#define YHOOK_OP_REPLACE 'r'
#define YHOOK_OP_DELETE 'd'
#define YHOOK_OP_MERGE 'm' // mean something changed
char *yhook_op_str(char op);

// flags for ynode hook and traverse func
#define YNODE_NO_FLAG 0x0
#define YNODE_LEAF_FIRST 0x1
#define YNODE_VAL_ONLY 0x2
#define YNODE_LEAF_ONLY 0x4
#define YNODE_SUPPRESS_HOOK 0x8

typedef void (*yhook_func0)(          char op, ynode *base, ynode *cur, ynode *_new);
typedef void (*yhook_func1)(void *U0, char op, ynode *base, ynode *cur, ynode *_new);
typedef void (*yhook_func2)(void *U0, char op, ynode *base, ynode *cur, ynode *_new, void *U1);
typedef void (*yhook_func3)(void *U0, char op, ynode *base, ynode *cur, ynode *_new, void *U1, void *U2);
typedef void (*yhook_func4)(void *U0, char op, ynode *base, ynode *cur, ynode *_new, void *U1, void *U2, void *U3);
typedef void (*yhook_func5)(void *U0, char op, ynode *base, ynode *cur, ynode *_new, void *U1, void *U2, void *U3, void *U4);
typedef void (*yhook_suppressed_func0)(          char op, ynode *base);
typedef void (*yhook_suppressed_func1)(void *U0, char op, ynode *base);
typedef void (*yhook_suppressed_func2)(void *U0, char op, ynode *base, void *U1);
typedef void (*yhook_suppressed_func3)(void *U0, char op, ynode *base, void *U1, void *U2);
typedef void (*yhook_suppressed_func4)(void *U0, char op, ynode *base, void *U1, void *U2, void *U3);
typedef void (*yhook_suppressed_func5)(void *U0, char op, ynode *base, void *U1, void *U2, void *U3, void *U4);

typedef yhook_func2 yhook_func;

// register the hook func to the target ynode.
ydb_res yhook_register(ynode *node, unsigned int flags, yhook_func func, int user_num, void *user[]);

// unregister the hook func from the target ynode.
// return user data registered with the hook.
void yhook_unregister(ynode *node);

typedef ydb_res (*ynode_callback)(ynode *cur, void *user);
ydb_res ynode_traverse(ynode *cur, ynode_callback cb, void *user, unsigned int flags);

#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#endif // __YNODE__
