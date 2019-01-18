#ifndef __YLIST__
#define __YLIST__

// YLIST is a simple double linked list working as queue or stack.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ylist ylist;

// ylist_iter is the iterator to loop each list entry.
typedef struct _ylist_iter ylist_iter;

#ifndef _USER_FREE_
#define _USER_FREE_
typedef void (*user_free)(void *);
#endif

// the callback definition to traverse all entries in the list.
// data: the data inserted.
// addition: the user-defined data
typedef int(*ylist_callback)(void *data, void *addition);
typedef void (*ylist_print)(void *data, void *addition);

// create a list.
ylist *ylist_create();

// destroy the list.
void ylist_destroy(ylist *list);

// destroy the list with free func for data.
void ylist_destroy_custom(ylist *list, user_free ufree);

// push the data into the head of the list. return ylist_iter if ok.
ylist_iter *ylist_push_front(ylist *list, void *data);

// push the data into the end of the list. return ylist_iter if ok.
ylist_iter *ylist_push_back(ylist *list, void *data);

// pop out of the first data of the list.
void *ylist_pop_front(ylist *list);

// pop out of the last data of the list.
void *ylist_pop_back(ylist *list);

// return 1 if it is empty.
int ylist_empty(ylist *list);

// return the number of entries in the list.
int ylist_size(ylist *list);

// just return the data of the first entry
void *ylist_front(ylist *list);

// just return the data of the last entry
void *ylist_back(ylist *list);

// get the ylist_iter of the first entry of the list.
ylist_iter *ylist_first(ylist *list);

// get the ylist_iter of the last entry of the list.
ylist_iter *ylist_last(ylist *list);

// return the next ylist_iter from the current ylist_iter.
ylist_iter *ylist_next(ylist *list, ylist_iter *iter);

// return the previous ylist_iter from the current ylist_iter.
ylist_iter *ylist_prev(ylist *list, ylist_iter *iter);

// return xth ylist_iter (index) from the list.
ylist_iter *ylist_index(ylist *list, int index);

// return 1 if the ylist_iter ended.
int ylist_done(ylist *list, ylist_iter *iter);

// get data of the ylist_iter.
void *ylist_data(ylist_iter *iter);

// remove the current ylist_iter and then return the previous ylist_iter.
// the data must be free if needed before ylist_erase
// or define and set ufree to free in progress.
ylist_iter *ylist_erase(ylist *list, ylist_iter *iter, user_free ufree);

// insert the data next to the ylist_iter and then 
// return new ylist_iter if ok or null if failed.
// if ylist_iter is null, the data will be pushed back to the list.
ylist_iter *ylist_insert(ylist *list, ylist_iter *iter, void *data);

// print all entries of the list.
void ylist_printf(ylist *list, ylist_print print, void *addition);

// traverse all entries of the list and call ylist_callback for each entry.
int ylist_traverse(ylist *list, ylist_callback cb, void *addition);

#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#endif // __YLIST__