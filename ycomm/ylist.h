#ifndef __YLIST__
#define __YLIST__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ylist ylist;
typedef struct _ylist_iter ylist_iter;

#ifndef _USER_FREE_
#define _USER_FREE_
typedef void (*user_free)(void *);
#endif

typedef int(*ylist_callback)(void *data, void *addition);
typedef void (*ylist_print)(void *data, void *addition);
typedef int (*ylist_cmp)(void *, void *);

// create a list
ylist *ylist_create();

// destroy the list
void ylist_destroy(ylist *list);

// destroy the list with free
void ylist_destroy_custom(ylist *list, user_free ufree);

// return ylist_iter if success
ylist_iter *ylist_push_front(ylist *list, void *data);

// return ylist_iter if success
ylist_iter *ylist_push_back(ylist *list, void *data);

// pop out data of the first entry if success
void *ylist_pop_front(ylist *list);

// pop out data of the last entry if success
void *ylist_pop_back(ylist *list);

// true if it is empty.
int ylist_empty(ylist *list);

// return the data of the first entry
void *ylist_front(ylist *list);

// return the data of the last entry
void *ylist_back(ylist *list);

// get the iterator of the list from head
ylist_iter *ylist_first(ylist *list);

// get the iterator of the list from tail
ylist_iter *ylist_last(ylist *list);

// return next iterator of the current iterator.
ylist_iter *ylist_next(ylist_iter *iter);

// return previous iterator of the current iterator.
ylist_iter *ylist_prev(ylist_iter *iter);

// true if the iterator ended
int ylist_done(ylist_iter *iter);

// get data of the iterator
void *ylist_data(ylist_iter *iter);

// delete the data of the iterator and then return next iterator.
ylist_iter *ylist_erase(ylist_iter *iter, user_free ufree);

// insert the data ahead of the iterator and then return new iterator for the data
// return NULL if it failed.
ylist_iter *ylist_insert(ylist_iter *iter, void *data);

void ylist_printf(ylist *list, ylist_print print, void *addition);

int ylist_traverse(ylist *list, ylist_callback cb, void *addition);
#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#endif // __YLIST__