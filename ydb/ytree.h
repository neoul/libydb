#ifndef __YTREE__
#define __YTREE__

// YTREE is a AVL tree that supports O(logn) search, insertion and deletion time.
// YTREE originated from the AVL tree implementation in https://rosettacode.org/wiki/AVL_tree/C.
// And tree node iteration and range search functions are added to the YTREE for convenient use.

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _USER_FREE_
#define _USER_FREE_
typedef void (*user_free)(void *);
#endif

// compare function for ytree construction
typedef int (*ytree_cmp)(void *, void *);

// print function for debug
typedef void (*ytree_print)(void *data);

// callback function for ytree iteration
typedef int(*ytree_callback)(void *key, void *data, void *addition);

typedef struct _ytree ytree;
typedef struct _ytree_node ytree_iter;

// create ytree with compare, key and data free functions.
ytree *ytree_create(ytree_cmp comp, user_free key_free);

// destroy the tree with deleting all entries.
void ytree_destroy(ytree *tree);
void ytree_destroy_custom(ytree *tree, user_free data_free);

// get the size of the tree
unsigned int ytree_size(ytree *tree);

// return NULL if ok, otherwise return old data.
void *ytree_insert(ytree *tree, void *key, void *data);

// insert the key and data without failure.
// data_free is used to remove old data.
void ytree_insert_custom(ytree *tree, void *key, void *data, user_free data_free);

// delete an entry and then return the found data, otherwise return NULL
void *ytree_delete(ytree *tree, void *key);

// delete data using user_free
void ytree_delete_custom(ytree *tree, void *key, user_free data_free);

// return the data if found, otherwise return NULL
void *ytree_search(ytree *tree, void *key);

// return 1 if found, otherwise return 0
int ytree_exist(ytree *tree, void *key);

// iterates all entries in the tree
int ytree_traverse(ytree *tree, ytree_callback cb, void *addition);

// iterates all entries in the tree in reverse direction
int ytree_traverse_reverse(ytree *tree, ytree_callback cb, void *addition);

// iterates all entries in the tree within a range.
int ytree_traverse_in_range(ytree *tree, 
    void *lower_boundary, void *higher_boundary, ytree_callback cb, void *addition);

// return the nearest iterator to the key. (This searches lower nodes if lower is set.)
ytree_iter *ytree_find_nearby(ytree *tree, void *key, int lower);
ytree_iter *ytree_find(ytree *tree, void *key);

ytree_iter *ytree_top(ytree *tree);
ytree_iter *ytree_first(ytree *tree);
ytree_iter *ytree_last(ytree *tree);
 
ytree_iter *ytree_prev(ytree *tree, ytree_iter *n);
ytree_iter *ytree_next(ytree *tree, ytree_iter *n);

// ytree_push --
//
//     Insert new key to the tree and return the iterator.
//     If the key exists in the tree, return old_data.
//
ytree_iter *ytree_push(ytree *tree, void *key, void *data, void **old_data);

// ytree_remove --
//
//     Remove the target iterator from the tree and then return the previous ytree iterator for next iteration.
//     The user data must be freed.
//
ytree_iter *ytree_remove(ytree *tree, ytree_iter *n, void **data);

// ytree_remove_reverse --
//
//     Remove the target iterator from the tree and then return the next ytree iterator for next iteration.
//     The user data must be freed.
//
ytree_iter *ytree_remove_reverse(ytree *tree, ytree_iter *n, void **data);

void *ytree_data(ytree_iter *n);
void *ytree_key(ytree_iter *n);

#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#endif // __YTREE__
