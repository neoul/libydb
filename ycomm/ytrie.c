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

// Iterates through the entries pairs in the map
int ytrie_traverse(ytrie *trie, ytrie_callback cb, void *data)
{
    art_callback art_cb = (art_callback) cb;
    return art_iter((art_tree *) trie, art_cb, data);
}

// Iterates through the entries pairs in the map
int ytrie_traverse_prefix(ytrie *trie, const void *prefix, int prefix_len, ytrie_callback cb, void *data)
{
    art_callback art_cb = (art_callback) cb;
    return art_iter_prefix((art_tree *) trie, (const unsigned char *)prefix, prefix_len, art_cb, data);
}

static int add_leaf(void *data, art_leaf *leaf)
{
    ytrie_iter *range;
    if(!data)
        return 1;
    range = data;
    range->list_iter = ylist_push_back(range->list, (void *) leaf);
    if(range->list_iter)
        return 0;
    return 1;
}

ytrie_iter* ytrie_iter_create(ytrie *trie, const void *prefix, int prefix_len)
{
    ytrie_iter *range;
    range = (ytrie_iter *) malloc(sizeof(ytrie_iter));
    if(range)
    {
        memset(range, 0x0, sizeof(ytrie_iter));
        range->trie = trie;
        range->list = ylist_create();
        if(range->list)
            art_iter_prefix_leaf((art_tree *) trie, (const unsigned char *)prefix, prefix_len, add_leaf, range);
        else
            fprintf(stderr, "  oops? - ytrie_iter_new seems to fail.\n");
        range->list_iter = ylist_first(range->list);
    }
    return range;
}

ytrie_iter* ytrie_iter_next(ytrie_iter *range)
{
    range->list_iter = ylist_next(range->list_iter);
    return range;
}

int ytrie_iter_done(ytrie_iter *range)
{
    if(range)
        return ylist_done(range->list_iter);
    return 1;
}


ytrie_iter* ytrie_iter_reset(ytrie_iter *range)
{
    range->list_iter = ylist_first(range->list);
    return range;
}

void ytrie_iter_delete(ytrie_iter *range)
{
    if(range)
    {
        if(range->list)
        {
            ylist_destroy(range->list);
            range->list = NULL;
        }
        free(range);
    }
}

void *ytrie_iter_get_data(ytrie_iter *range)
{
    art_leaf *leaf = ylist_data(range->list_iter);
    if(leaf)
        return leaf->value;
    return NULL;
}

const void *ytrie_iter_get_key(ytrie_iter *range)
{
    art_leaf *leaf = ylist_data(range->list_iter);
    if(leaf)
        return (const void *)leaf->key;
    return NULL;
}

int ytrie_iter_get_key_len(ytrie_iter *range)
{
    art_leaf *leaf = ylist_data(range->list_iter);
    if(leaf)
        return leaf->key_len;
    return 0;
}
