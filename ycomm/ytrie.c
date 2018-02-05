#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "art.h"
#include "ytrie.h"
#include "yque.h"

ytrie ytrie_create()
{
    art_tree *art;
    art = malloc(sizeof(art_tree));
    if(art)
    {
        art_tree_init(art);
    }
    return (ytrie) art;
}

void ytrie_destroy(ytrie trie)
{
    if(trie)
        art_tree_destroy(trie);
    free(trie);
}

void ytrie_destroy_custom(ytrie trie, user_free data_free)
{
    if(trie)
        art_tree_destroy_custom(trie, data_free);
    free(trie);
}

size_t ytrie_size(ytrie trie)
{
    return (size_t) art_size(trie);
}

// return NULL if ok, otherwise return old value
void *ytrie_insert(ytrie trie, const void *key, int key_len, void *value)
{
    return art_insert(trie, (const unsigned char *)key, key_len, value);
}

// delete the key and return the found value, otherwise return NULL
void *ytrie_delete(ytrie trie, const void *key, int key_len)
{
    return art_delete(trie, (const unsigned char *)key, key_len);
}

// return the value if found, otherwise return NULL
void *ytrie_search(ytrie trie, const void *key, int key_len)
{
    return art_search(trie, (const unsigned char *)key, key_len);
}

// Iterates through the entries pairs in the map
int ytrie_traverse(ytrie trie, ytrie_callback cb, void *data)
{
    art_callback art_cb = (art_callback) cb;
    return art_iter(trie, art_cb, data);
}

// Iterates through the entries pairs in the map
int ytrie_traverse_prefix(ytrie trie, const void *prefix, int prefix_len, ytrie_callback cb, void *data)
{
    art_callback art_cb = (art_callback) cb;
    art_iter_prefix(trie, (const unsigned char *)prefix, prefix_len, art_cb, data);
}

static int _range_cb(void *data, art_leaf *leaf)
{
    ytrie_iter *range;
    if(!data)
        return 1;
    range = data;
    yque_push_back(range->que, (void *) leaf);
}

ytrie_iter* ytrie_iter_new(ytrie trie, const void *prefix, int prefix_len)
{
    ytrie_iter *range;
    range = (ytrie_iter *) malloc(sizeof(ytrie_iter));
    if(range)
    {
        memset(range, 0x0, sizeof(ytrie_iter));
        range->trie = trie;
        range->que = yque_create();
        if(range->que)
            art_iter_prefix_leaf(trie, (const unsigned char *)prefix, prefix_len, _range_cb, range);
        else
            fprintf(stderr, "  oops? - ytrie_iter_new seems to fail.\n");
        ytrie_iter_next(range);
    }
    return range;
}

ytrie_iter* ytrie_iter_next(ytrie_iter *range)
{
    if(!range || !range->que)
        return range;
    if(range->que_iter)
    {
        yque_iter_next(range->que_iter);
    }
    else
    {
        range->que_iter = yque_iter_new(range->que);
        if(!range->que_iter)
            return range;
    }
    if(yque_iter_done(range->que, range->que_iter))
    {
        yque_iter_delete(range->que_iter);
        range->que_iter = NULL;
    }
    return range;
}

ytrie_iter* ytrie_iter_reset(ytrie_iter *range)
{
    if(!range || !range->que)
        return range;
    if(range->que_iter)
    {
        yque_iter_delete(range->que_iter);
        range->que_iter = NULL;
    }
    ytrie_iter_next(range);
    return range;
}

void ytrie_iter_delete(ytrie_iter *range)
{
    if(range)
    {
        if(range->que_iter)
        {
            yque_iter_delete(range->que_iter);
            range->que_iter = NULL;
        }
        if(range->que)
        {
            yque_destroy(range->que);
            range->que = NULL;
        }
        free(range);
    }
}

void *ytrie_iter_get_data(ytrie_iter *range)
{
    art_leaf *leaf;
    if(!range->que_iter)
        return NULL;
    leaf = yque_iter_data(range->que_iter);
    if(leaf)
        return leaf->value;
    return NULL;
}

const void *ytrie_iter_get_key(ytrie_iter *range)
{
    art_leaf *leaf;
    if(!range->que_iter)
        return NULL;
    leaf = yque_iter_data(range->que_iter);
    if(leaf)
        return (const void *)leaf->key;
    return NULL;
}

int ytrie_iter_get_key_len(ytrie_iter *range)
{
    art_leaf *leaf;
    if(!range->que_iter)
        return 0;
    leaf = yque_iter_data(range->que_iter);
    if(leaf)
        return leaf->key_len;
    return 0;
}
