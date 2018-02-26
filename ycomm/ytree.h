#ifndef __YTREE__
#define __YTREE__

#ifndef _USER_FREE_
#define _USER_FREE_
typedef void (*user_free)(void *);
#endif

typedef void *ytree;
typedef void *ynode;
typedef int (*ytree_cmp)(void *, void *);
typedef void (*ytree_print)(void *);

// callback function for ytree iteration
typedef int(*ytree_callback)(void *data, void *addition);

ytree ytree_create(ytree_cmp cmp, ytree_print print);
void ytree_destroy(ytree tree);
void ytree_destroy_custom(ytree tree, user_free data_free);
unsigned int ytree_size(ytree tree);
// return NULL if ok, otherwise return old value
void *ytree_insert(ytree tree, void *data);
// delete the data and then return the found data, otherwise return NULL
void *ytree_delete(ytree tree, void *data);
// return the value if found, otherwise return NULL
void *ytree_search(ytree tree, void *data);
// Iterates through entries in the tree
int ytree_traverse(ytree tree, ytree_callback cb, void *addition);
// Iterates through entries in the tree in reverse direction
int ytree_traverse_reverse(ytree tree, ytree_callback cb, void *addition);
// Iterates through the entries in the tree within a range.
int ytree_traverse_in_range(ytree tree, void *lower_boundary_data, void *higher_boundary_data, ytree_callback cb, void *addition);

void ytree_printf(ytree tree);

ynode ytree_top(ytree tree);
ynode ytree_first(ytree tree);
ynode ytree_last(ytree tree);
 
ynode  ytree_prev(ytree tree, ynode n);
ynode  ytree_next(ytree tree, ynode n);

void *ytree_data(ynode n);

#endif // __YTREE__
