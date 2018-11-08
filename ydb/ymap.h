#ifndef __YMAP__
#define __YMAP__

// ymap is the ordered map constructed by yarray and 
// ytree in order to support YAML ordered map.
// - the data ordered by the insert sequence.
// - each data is unique by the key in ymap.
// - fast search by the key.

#include "ytree.h"
#include "ylist.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct _ymap ymap;
// ymap_iter is the iterator to loop each map entry and access them.
typedef struct _ymap_iter ymap_iter;

// create a ymap
ymap *ymap_create(ytree_cmp comp, user_free kfree);
// destroy the ymap with deleting all entries.
void ymap_destroy_custom(ymap *map, user_free ufree);
void ymap_destroy(ymap *map);

// get the size of the map
unsigned int ymap_size(ymap *map);

// insert the key and data into the tail of the ymap 
// and then return NULL if ok, otherwise return old inserted data.
// null key is not inserted.
void *ymap_insert_back(ymap *map, void *key, void *data);
// insert the key and data into the head of the ymap 
// and then return NULL if ok, otherwise return old inserted data.
// null key is not inserted.
void *ymap_insert_front(ymap *map, void *key, void *data);
// delete the key from the ymap return the data.
void *ymap_delete(ymap *map, void *key);

// return the data if found, otherwise return NULL
void *ymap_search(ymap *map, void *key);
// return 1 if found, otherwise return 0
int ymap_exist(ymap *map, void *key);

// iterates all entries in the ymap regardless of ordering.
int ymap_traverse(ymap *map, ytree_callback cb, void *addition);
// iterates all entries in the ymap in ordering.
int ymap_traverse_order(ymap *map, ytree_callback cb, void *addition);

// get the iterator of the ymap from the head
ymap_iter *ymap_first(ymap *map);
// get the iterator of the ymap from the head
ymap_iter *ymap_last(ymap *map);
// get the previous iterator of the iterator.
ymap_iter *ymap_prev(ymap *map, ymap_iter *imap);
// get the next iterator of the iterator.
ymap_iter *ymap_next(ymap *map, ymap_iter *imap);
// return 1 if the iterator is done.
int ymap_done(ymap *map, ymap_iter *imap);

// remove the ymap_iter and return the previous ymap_iter.
// the data must be free if needed before ymap_remove
// or define and set ufree function to free.
ymap_iter *ymap_remove(ymap *map, ymap_iter *imap, user_free ufree);
// insert the data next to the ymap_iter and then 
// return new ymap_iter for the inserted data if ok or null if failed.
// if ymap_iter is null, the data will be pushed back to the list.
// if there is the same key exists in the ymap, it will be failed.
ymap_iter *ymap_insert(ymap *map, ymap_iter *imap, void *key, void *data);

// return the key of the ymap_iter
void *ymap_key(ymap_iter *imap);
// return the data of the ymap_iter
void *ymap_data(ymap_iter *imap);
// return xth ymap_iter (index) from the ymap.
ymap_iter *ymap_index(ymap *map, int index);

#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#endif // __YMAP__