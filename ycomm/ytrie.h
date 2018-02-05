#ifndef __YTRIE__
#define __YTRIE__

#include <stddef.h>
#include "yque.h"

#ifndef _USER_FREE_
#define _USER_FREE_
typedef void (*user_free)(void *);
#endif

typedef void *ytrie;

ytrie ytrie_create();

void ytrie_destroy(ytrie trie);

void ytrie_destroy_custom(ytrie trie, user_free);

size_t ytrie_size(ytrie trie);

// return NULL if ok, otherwise return old value
void *ytrie_insert(ytrie trie, const void *key, int key_len, void *value);

// delete the key and return the found value, otherwise return NULL
void *ytrie_delete(ytrie trie, const void *key, int key_len);

// return the value if found, otherwise return NULL
void *ytrie_search(ytrie trie, const void *key, int key_len);

// callback function for ytrie iteration
typedef int(*ytrie_callback)(void *data, const void *key, int key_len, void *value);


// Iterates through the entries pairs in the map
int ytrie_traverse(ytrie trie, ytrie_callback cb, void *data);

// Iterates through the entries pairs in the map
int ytrie_traverse_prefix(ytrie trie, const void *prefix, int prefix_len, ytrie_callback cb, void *data);


typedef struct ytrie_iter_s
{
    ytrie trie;
    yque que;
    yque_iter que_iter;
} ytrie_iter;

ytrie_iter* ytrie_iter_new(ytrie trie, const void *prefix, int prefix_len);
ytrie_iter* ytrie_iter_next(ytrie_iter *iter);
ytrie_iter* ytrie_iter_reset(ytrie_iter *iter);
void ytrie_iter_delete(ytrie_iter *iter);

void *ytrie_iter_get_data(ytrie_iter *iter);
const void *ytrie_iter_get_key(ytrie_iter *iter);
int ytrie_iter_get_key_len(ytrie_iter *iter);


// ytrie_iter* iter = ytrie_iter_new(trie, prefix, prefix_len);
// for(; iter && iter->que_iter; iter=ytrie_iter_next(iter))
// {  }
// ytrie_iter_delete(iter);

#endif // __YTRIE__


// #ifdef YTRIE_STRING
// #define ytrie_insert(T, K, V) ytrie_insert((T), (K), strlen(K), (V))
// #endif

// #ifdef YTRIE_INTEGER
// #include <arpa/inet.h>
// #define ytrie_insert(T, K, V) ytrie_insert((T), &(htonl(K)), sizeof(uint32_t), (V))
// #endif

// use case for YTRIE_STRING
// #define YTRIE_INTEGER
// include "ytrie.h"

