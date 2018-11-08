#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

#include "ymap.h"

struct _ymap
{
    ytree *tree;
    ylist *list;
};

struct _ymap_iter
{
    ylist_iter *ilist;
    void *key;
    void *data;
};

// create a ymap
ymap *ymap_create(ytree_cmp comp, user_free kfree)
{
    ymap *map;
    map = malloc(sizeof(ymap));
    if (map)
    {
        map->list = ylist_create();
        if (!map->list)
        {
            free(map);
            return NULL;
        }
        map->tree = ytree_create(comp, kfree);
        if (!map->tree)
        {
            ylist_destroy(map->list);
            free(map);
            return NULL;
        }
    }
    return map;
}

// destroy the ymap with deleting all entries.
void ymap_destroy_custom(ymap *map, user_free ufree)
{
    if (map)
    {
        ytree_destroy(map->tree);
        while (!ylist_empty(map->list))
        {
            ymap_iter *imap;
            imap = ylist_pop_front(map->list);
            if (ufree)
                ufree(imap->data);
            free(imap);
        }
        ylist_destroy(map->list);
        free(map);
    }
}
void ymap_destroy(ymap *map)
{
    ymap_destroy_custom(map, NULL);
}

// get the size of the map
unsigned int ymap_size(ymap *map)
{
    if (map)
        return ytree_size(map->tree);
    return 0;
}

// insert the key and data into the tail of the ymap
// and then return NULL if ok, otherwise return old inserted data.
// null key or value are not inserted.
void *ymap_insert_back(ymap *map, void *key, void *data)
{
    if (map && key)
    {
        ymap_iter *imap;
        imap = malloc(sizeof(ymap_iter));
        assert(imap);
        imap->key = key;
        imap->data = data;
        imap->ilist = ylist_push_back(map->list, imap);
        assert(imap->ilist);
        imap = ytree_insert(map->tree, key, imap);
        if (imap) // old imap
        {
            data = imap->data;
            ylist_erase(map->list, imap->ilist, NULL);
            free(imap);
            return data;
        }
    }
    return NULL;
}

// insert the key and data into the head of the ymap
// and then return NULL if ok, otherwise return old inserted data.
// null key or value are not inserted.
void *ymap_insert_front(ymap *map, void *key, void *data)
{
    if (map && key)
    {
        ymap_iter *imap;
        imap = malloc(sizeof(ymap_iter));
        assert(imap);
        imap->key = key;
        imap->data = data;
        imap->ilist = ylist_push_front(map->list, imap);
        assert(imap->ilist);
        imap = ytree_insert(map->tree, key, imap);
        if (imap) // old imap
        {
            data = imap->data;
            ylist_erase(map->list, imap->ilist, NULL);
            free(imap);
            return data;
        }
    }
    return NULL;
}

// delete the key from the ymap return the data.
void *ymap_delete(ymap *map, void *key)
{
    if (map && key)
    {
        ymap_iter *imap;
        imap = ytree_delete(map->tree, key);
        if (imap)
        {
            void *data = imap->data;
            ylist_erase(map->list, imap->ilist, NULL);
            free(imap);
            return data;
        }
    }
    return NULL;
}

// return the data if found, otherwise return NULL
void *ymap_search(ymap *map, void *key)
{
    if (map && key)
    {
        ymap_iter *imap;
        imap = ytree_search(map->tree, key);
        if (imap)
            return imap->data;
    }
    return NULL;
}

// return 1 if found, otherwise return 0
int ymap_exist(ymap *map, void *key)
{
    if (map)
    {
        ymap_iter *imap;
        imap = ytree_search(map->tree, key);
        if (imap)
            return 1;
    }
    return 0;
}

struct _ymap_tr_data
{
    ytree_callback cb;
    void *addition;
};

static int ymap_traverse_sub(void *key, void *data, void *addition)
{
    ymap_iter *imap = data;
    struct _ymap_tr_data *pdata = addition;
    assert(pdata || imap);
    return pdata->cb(key, imap->data, pdata->addition);
}

static int ymap_traverse_order_sub(void *data, void *addition)
{
    ymap_iter *imap = data;
    struct _ymap_tr_data *pdata = addition;
    assert(pdata || imap);
    return pdata->cb(imap->key, imap->data, pdata->addition);
}

// iterates all entries in the ymap regardless of ordering.
int ymap_traverse(ymap *map, ytree_callback cb, void *addition)
{
    if (map)
    {
        struct _ymap_tr_data data;
        data.cb = cb;
        data.addition = addition;
        return ytree_traverse(map->tree, ymap_traverse_sub, &data);
    }
    return 1;
}

// iterates all entries in the ymap in ordering.
int ymap_traverse_order(ymap *map, ytree_callback cb, void *addition)
{
    if (map)
    {
        struct _ymap_tr_data data;
        data.cb = cb;
        data.addition = addition;
        return ylist_traverse(map->list, ymap_traverse_order_sub, &data);
    }
    return 1;
}

// get the iterator of the ymap from the head
ymap_iter *ymap_first(ymap *map)
{
    if (map)
    {
        ylist_iter *ilist;
        ilist = ylist_first(map->list);
        return ylist_data(ilist);
    }
    return NULL;
}

// get the iterator of the ymap from the head
ymap_iter *ymap_last(ymap *map)
{
    if (map)
    {
        ylist_iter *ilist;
        ilist = ylist_last(map->list);
        return ylist_data(ilist);
    }
    return NULL;
}

// get the previous iterator of the iterator.
ymap_iter *ymap_prev(ymap *map, ymap_iter *imap)
{
    if (map && imap)
    {
        ylist_iter *ilist;
        ilist = ylist_prev(map->list, imap->ilist);
        return ylist_data(ilist);
    }
    return NULL;
}

// get the next iterator of the iterator.
ymap_iter *ymap_next(ymap *map, ymap_iter *imap)
{
    if (map)
    {
        ylist_iter *ilist;
        if (imap)
        {
            ilist = ylist_next(map->list, imap->ilist);
            return ylist_data(ilist);
        }
        else
        {
            ilist = ylist_first(map->list);
            return ylist_data(ilist);
        }
    }
    return NULL;
}

// return 1 if the iterator is done.
int ymap_done(ymap *map, ymap_iter *imap)
{
    if (map && imap)
    {
        return ylist_done(map->list, imap->ilist);
    }
    return 1;
}

// remove the ymap_iter and return the previous ymap_iter.
// the data must be free if needed before ymap_remove
// or define and set ufree function to free.
ymap_iter *ymap_remove(ymap *map, ymap_iter *imap, user_free ufree)
{
    if (map && imap)
    {
        void *data;
        ylist_iter *prev;
        ymap_iter *imap_copy;
        imap_copy = ytree_delete(map->tree, imap->key);
        assert(imap_copy == imap);
        prev = ylist_erase(map->list, imap->ilist, NULL);
        data = imap->data;
        free(imap);
        if (ufree)
            ufree(data);
        imap = ylist_data(prev);
        return imap;
    }
    return NULL;
}

// insert the data next to the ymap_iter and then 
// return new ymap_iter for the inserted data if ok or null if failed.
// if ymap_iter is null, the data will be pushed back to the list.
// if there is the same key exists in the ymap, it will be failed.
ymap_iter *ymap_insert(ymap *map, ymap_iter *imap, void *key, void *data)
{
    if (map && key)
    {
        ymap_iter *new_imap;
        new_imap = ytree_search(map->tree, key);
        if (new_imap)
            return NULL;
        new_imap = malloc(sizeof(ymap_iter));
        assert(new_imap);
        new_imap->key = key;
        new_imap->data = data;
        new_imap->ilist = ylist_insert(map->list, imap->ilist, new_imap);
        assert(new_imap->ilist);
        imap = ytree_insert(map->tree, key, new_imap);
        assert(!imap);
        return new_imap;
    }
    return NULL;
}

// return the key of the ymap_iter
void *ymap_key(ymap_iter *imap)
{
    if (imap)
        return imap->key;
    return NULL;
}

// return the data of the ymap_iter
void *ymap_data(ymap_iter *imap)
{
    if (imap)
        return imap->data;
    return NULL;
}

// return xth ymap_iter (index) from the ymap.
ymap_iter *ymap_index(ymap *map, int index)
{
    ylist_iter *ilist;
    if (!map)
        return NULL;
    ilist = ylist_index(map->list, index);
    return ylist_data(ilist);
}
