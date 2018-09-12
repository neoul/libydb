#ifndef __YNODE__
#define __YNODE__

// low end_level function for ydb

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
void ynode_dump_node(FILE *fp, int fd, char *buf, int buflen, ynode *node, int start_level, int end_level);
#define ynode_dump_to_fp(fp, node, start_level, end_level) ynode_dump_node(fp, 0, NULL, 0, node, start_level, end_level);
#define ynode_dump_to_buf(buf, buflen, node, start_level, end_level) ynode_dump_node(NULL, 0, buf, buflen, node, start_level, end_level);
#define ynode_dump_to_fd(fd, node, start_level, end_level) ynode_dump_node(NULL, fd, NULL, 0, node, start_level, end_level);
#define ynode_dump(node, start_level, end_level) ynode_dump_node(stdout, 0, NULL, 0, node, start_level, end_level);

// write ynode db to buffer , fp, fd
int ynode_snprintf(char *buf, int buflen, ynode *node, int start_level, int end_level);
int ynode_fprintf(FILE *fp, ynode *node, int start_level, int end_level);
int ynode_write(int fd, ynode *node, int start_level, int end_level);
int ynode_printf(ynode *node, int start_level, int end_level);

// read ynode db from buffer , fp, fd
ynode *ynode_fscanf(FILE *fp);
ynode *ynode_scanf();
ynode *ynode_read(int fd);
ynode *ynode_sscanf(char *buf, int buflen);

// [ynode searching & traveling]

// lookup the ynode in the path
ynode *ynode_search(ynode *node, char *path);

// return ynodes' value if that is a leaf.
char *ynode_data(ynode *node);
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
// the start_level is the number of the parent and ancestors to be printed.
// the path returned should be free.
char *ynode_path(ynode *node, int start_level);

// create a new path and value string for the ydb
char *ynode_path_and_val(ynode *node, int start_level);

// ynode CRUD (Create, Read, Update(merge/replace/overwrite), Delete) operation API
// create single ynode
ynode *ynode_create(ynode *parent, unsigned char type, char *key, char *value);

// create ynode db using path
// return the last created ynode.
ynode *ynode_create_path(ynode *parent, char *path);

// create new ynode db (all sub nodes).
// ynode_clone and ynode_copy return the same result. but, implemented with different logic.
ynode *ynode_clone(ynode *src);
ynode *ynode_copy(ynode *src);

// merge src ynode to dest node.
// dest will be modified by the operation.
ynode *ynode_overwrite(ynode *dest, ynode *src);

// replace dest ynode db using src ynode.
// only update the dest ynode value (leaf).
ynode *ynode_replace(ynode *dest, ynode *src);

// merge src ynode to dest node.
// dest and src ynodes will not be modified.
// New ynode db will returned.
ynode *ynode_merge(ynode *dest, ynode *src);

// delete the ynode db (including all sub nodes).
void ynode_delete(ynode *node);

// ynode callback for hooking some change in ynode db.


#endif // __YNODE__
