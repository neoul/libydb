#ifndef __YTRIE__
#define __YTRIE__

// YTRIE is adaptive radix tree (prefix tree) based on https://github.com/armon/libart
// YTRIE is implemented from the base algorithm with addtitional facilities as follows.
//  - supports exact and best matching search.
//  - supports the callback function to traverse all nodes in YTRIE.
//  - supports range search.

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#ifndef _USER_FREE_
#define _USER_FREE_
typedef void (*user_free)(void *);
#endif

typedef struct _ytree ytrie;

ytrie *ytrie_create(void);

void ytrie_destroy(ytrie *trie);

void ytrie_destroy_custom(ytrie *trie, user_free);

size_t ytrie_size(ytrie *trie);

// return NULL if ok, otherwise return old value
void *ytrie_insert(ytrie *trie, const void *key, int key_len, void *value);

// delete the key and return the found value, otherwise return NULL
void *ytrie_delete(ytrie *trie, const void *key, int key_len);

// return the value if found, otherwise return NULL
void *ytrie_search(ytrie *trie, const void *key, int key_len);

// return the best matched value if found, otherwise return NULL
void *ytrie_best_match(ytrie *trie, const void *key, int key_len, int *matched_len);

// callback function for ytrie *iteration
typedef int(*ytrie_callback)(void *addition, const void *key, int key_len, void *value);

// Iterates through the entries pairs in the trie
int ytrie_traverse(ytrie *trie, ytrie_callback cb, void *addition);

// Iterates through the entries pairs in the trie
int ytrie_traverse_prefix_match(ytrie *trie, const void *key, int key_len, ytrie_callback cb, void *addition);

#include "ylist.h"
ylist* ytrie_search_range(ytrie *trie, const void *key, int key_len);

#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#endif // __YTRIE__