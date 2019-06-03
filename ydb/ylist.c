
#include <stdio.h>
#include <stdlib.h>
#include "ylist.h"

struct _ylist_iter
{
    struct _ylist_iter *next;
    struct _ylist_iter *prev;
    void *data;
};

struct _ylist
{
    struct _ylist_iter _head;
    struct _ylist_iter *head;
    size_t size;
};

// create a list.
ylist *ylist_create(void)
{
    struct _ylist *list = malloc(sizeof(struct _ylist));
    if (list)
    {
        list->head = &(list->_head);
        list->head->next = list->head;
        list->head->prev = list->head;
        list->head->data = NULL;
        list->size = 0;
    }
    return list;
}

// destroy the list.
void ylist_destroy(ylist *list)
{
    if (list)
    {
        void *data = NULL;
        do
        {
            data = ylist_pop_front(list);
        } while (data);
        free(list);
    }
}

// destroy the list with free func for data.
void ylist_destroy_custom(ylist *list, user_free ufree)
{
    if (list)
    {
        void *data = NULL;
        do
        {
            data = ylist_pop_front(list);
            if (data && ufree)
                ufree(data);
        } while (data);
        free(list);
    }
}

// push the data into the head of the list. return ylist_iter if ok.
ylist_iter *ylist_push_front(ylist *list, void *data)
{
    struct _ylist_iter *iter;
    if (!list)
        return NULL;
    iter = malloc(sizeof(struct _ylist_iter));
    if (iter)
    {
        iter->data = data;
        iter->next = list->head->next;
        iter->prev = list->head;
        list->head->next->prev = iter;
        list->head->next = iter;
        list->size++;
    }
    return iter;
}

// push the data into the end of the list. return ylist_iter if ok.
ylist_iter *ylist_push_back(ylist *list, void *data)
{
    struct _ylist_iter *iter;
    if (!list)
        return NULL;
    iter = malloc(sizeof(struct _ylist_iter));
    if (iter)
    {
        iter->data = data;
        iter->next = list->head;
        iter->prev = list->head->prev;
        list->head->prev->next = iter;
        list->head->prev = iter;
        list->size++;
    }
    return iter;
}

// pop out of the first data of the list.
void *ylist_pop_front(ylist *list)
{
    struct _ylist_iter *iter;
    if (!list)
        return NULL;
    iter = list->head->next;
    if (iter == list->head)
        return NULL;
    else
    {
        void *data = iter->data;
        iter->next->prev = list->head;
        list->head->next = iter->next;
        list->size--;
        free(iter);
        return data;
    }
}

// pop out of the last data of the list.
void *ylist_pop_back(ylist *list)
{
    struct _ylist_iter *iter;
    if (!list)
        return NULL;
    iter = list->head->prev;
    if (iter == list->head)
        return NULL;
    else
    {
        void *data = iter->data;
        iter->prev->next = list->head;
        list->head->prev = iter->prev;
        list->size--;
        free(iter);
        return data;
    }
}

// return 1 if it is empty.
int ylist_empty(ylist *list)
{
    if (list)
    {
        if (list->head->next == list->head)
            return 1;
        if (list->head->prev == list->head)
            return 1;
        return 0;
    }
    return 1;
}

// return the number of entries in the list.
int ylist_size(ylist *list)
{
    if (list)
        return list->size;
    return 0;
}

// just return the data of the first entry
void *ylist_front(ylist *list)
{
    struct _ylist_iter *iter;
    if (!list)
        return NULL;
    iter = list->head->next;
    if (iter == list->head)
        return NULL;
    else
        return iter->data;
}

// just return the data of the last entry
void *ylist_back(ylist *list)
{
    struct _ylist_iter *iter;
    if (!list)
        return NULL;
    iter = list->head->prev;
    if (iter == list->head)
        return NULL;
    else
        return iter->data;
}

// get the ylist_iter of the first entry of the list.
ylist_iter *ylist_first(ylist *list)
{
    if (!list)
        return NULL;
    if (list->head->next == list->head)
        return NULL;
    return list->head->next;
}

// get the ylist_iter of the last entry of the list.
ylist_iter *ylist_last(ylist *list)
{
    if (!list)
        return NULL;
    if (list->head->prev == list->head)
        return NULL;
    return list->head->prev;
}

// return the next ylist_iter from the current ylist_iter.
ylist_iter *ylist_next(ylist *list, ylist_iter *iter)
{
    if (!list || !iter)
        return NULL;
    if (iter->next == list->head)
        return NULL;
    return iter->next;
}

// return the previous ylist_iter from the current ylist_iter.
ylist_iter *ylist_prev(ylist *list, ylist_iter *iter)
{
    if (!list || !iter)
        return NULL;
    if (iter->prev == list->head)
        return NULL;
    return iter->prev;
}

// return xth ylist_iter (index) from the list.
ylist_iter *ylist_index(ylist *list, int index)
{
    int count = 0;
    ylist_iter *iter;
    if (!list)
        return NULL;
    if (list->head->next == list->head)
        return NULL;
    iter = list->head->next; // 0th
    while (iter) {
        if (count == index)
            return iter;
        iter = ylist_next(list, iter);
        count++;
    }
    return iter;
}

// return 1 if the ylist_iter ended.
int ylist_done(ylist *list, ylist_iter *iter)
{
    if (!iter || !list)
        return 1;
    if (iter == list->head)
        return 1;
    return 0;
}

// get data of the ylist_iter.
void *ylist_data(ylist_iter *iter)
{
    if (iter)
        return iter->data;
    return NULL;
}

// remove the current ylist_iter and then return the previous ylist_iter.
// the data must be free if needed before ylist_erase
// or define and set ufree to free in progress.
ylist_iter *ylist_erase(ylist *list, ylist_iter *iter, user_free ufree)
{
    struct _ylist_iter *prev;
    if (!iter || !list)
        return NULL;
    if (iter == list->head)
        return NULL;
    prev = iter->prev;
    if (ufree)
        ufree(iter->data);
    iter->prev->next = iter->next;
    iter->next->prev = iter->prev;
    free(iter);
    list->size--;
    return prev;
}

// insert the data next to the ylist_iter and then 
// return new ylist_iter if ok or null if failed.
// if ylist_iter is null, the data will be pushed back to the list.
ylist_iter *ylist_insert(ylist *list, ylist_iter *iter, void *data)
{
    struct _ylist_iter *new_iter;
    if (!list)
        return NULL;
    if (!iter)
        return ylist_push_back(list, data);

    new_iter = malloc(sizeof(struct _ylist_iter));
    if (new_iter)
    {
        new_iter->data = data;
        new_iter->next = iter->next;
        new_iter->prev = iter;
        iter->next->prev = new_iter;
        iter->next = new_iter;
        list->size++;
    }
    return new_iter;
}

// print all entries of the list.
void ylist_printf(ylist *list, ylist_print print, void *addition)
{
    struct _ylist_iter *iter;
    if (!list)
        return;
    for (iter = list->head->next; iter != list->head; iter = iter->next)
    {
        print(iter->data, addition);
    }
}

// traverse all entries of the list and call ylist_callback for each entry.
int ylist_traverse(ylist *list, ylist_callback cb, void *addition)
{
    int res = -1;
    struct _ylist_iter *iter;
    if (!list)
        return res;
    for (iter = list->head->next; iter != list->head; iter = iter->next)
    {
        res = cb(iter->data, addition);
        if(res)
            return res;
    }
    return res;
}

