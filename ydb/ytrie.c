#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "art.h"
#include "ytrie.h"
#include "ylist.h"

ytrie *ytrie_create()
{
    art_tree *art;
    art = malloc(sizeof(art_tree));
    if(art)
    {
        art_tree_init(art);
    }
    return (ytrie *) art;
}

void ytrie_destroy(ytrie *trie)
{
    if(trie)
        art_tree_destroy((art_tree *) trie);
    free(trie);
}

void ytrie_destroy_custom(ytrie *trie, user_free data_free)
{
    if(trie)
        art_tree_destroy_custom((art_tree *) trie, data_free);
    free(trie);
}

size_t ytrie_size(ytrie *trie)
{
    return (size_t) art_size((art_tree *) trie);
}

// return NULL if ok, otherwise return old value
void *ytrie_insert(ytrie *trie, const void *key, int key_len, void *value)
{
    return art_insert((art_tree *) trie, (const unsigned char *)key, key_len, value);
}

// delete the key and return the found value, otherwise return NULL
void *ytrie_delete(ytrie *trie, const void *key, int key_len)
{
    return art_delete((art_tree *) trie, (const unsigned char *)key, key_len);
}

// return the value if found, otherwise return NULL
void *ytrie_search(ytrie *trie, const void *key, int key_len)
{
    return art_search((art_tree *) trie, (const unsigned char *)key, key_len);
}

// return the best matched value if found, otherwise return NULL
void *ytrie_best_match(ytrie *trie, const void *key, int key_len, int *matched_len)
{
    return art_best_match((art_tree *) trie, (const unsigned char *)key, key_len, matched_len);
}

// Iterates through the entries pairs in the map
int ytrie_traverse(ytrie *trie, ytrie_callback cb, void *data)
{
    art_callback art_cb = (art_callback) cb;
    return art_iter((art_tree *) trie, art_cb, data);
}

// Iterates through the entries pairs in the map
int ytrie_traverse_prefix_match(ytrie *trie, const void *prefix, int prefix_len, ytrie_callback cb, void *data)
{
    art_callback art_cb = (art_callback) cb;
    return art_iter_prefix((art_tree *) trie, (const unsigned char *)prefix, prefix_len, art_cb, data);
}

static int add_data(void *data, art_leaf *leaf)
{
    ylist *list = data;
    if(!data || !leaf)
        return 1;
    ylist_iter *iter = ylist_push_back(list, (void *) leaf->value);
    if(iter)
        return 0;
    return 1;
}

ylist* ytrie_search_range(ytrie *trie, const void *key, int key_len)
{
    ylist *list = ylist_create();
    int res = art_iter_prefix_leaf((art_tree *) trie, 
        (const unsigned char *)key, key_len, add_data, list);
    if(res) {
        ylist_destroy(list);
        return NULL;
    }
    return list;
}

