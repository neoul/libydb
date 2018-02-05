#ifndef __YQUEUE__
#define __YQUEUE__

#ifdef __cplusplus
extern "C" {
#endif

typedef void *yque;
typedef void *yque_iter;

#ifndef _USER_FREE_
#define _USER_FREE_
typedef void (*user_free)(void *);
#endif

yque yque_create();
void yque_destroy(yque que);
void yque_destroy_custom(yque que, user_free free);
void yque_push_front(yque que, void *data);
void yque_push_back(yque que, void *data);
void *yque_pop_front(yque que);
void *yque_pop_back(yque que);
int yque_size(yque que);
int yque_empty(yque que);
void *yque_front(yque que);
void *yque_back(yque que);
yque_iter yque_iter_new(yque que);
yque_iter yque_iter_next(yque_iter iter);
yque_iter yque_iter_prev(yque_iter iter);
yque_iter yque_iter_begin(yque que, yque_iter iter);
int yque_iter_done(yque que, yque_iter iter);
void *yque_iter_data(yque_iter iter);
void yque_erase(yque que, yque_iter iter);
void yque_erase_custom(yque que, yque_iter iter, user_free free);
void yque_insert(yque que, yque_iter iter, void *data);
void yque_iter_delete(yque_iter iter);

#ifdef __cplusplus
} // closing brace for extern "C"

#endif

#endif // __YQUEUE__
