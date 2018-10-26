
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

// create a list
ylist *ylist_create()
{
    struct _ylist *list = malloc(sizeof(struct _ylist));
    if (list)
    {
        list->head = &(list->_head); // malloc(sizeof(struct _ylist_iter));
        if (!list->head)
        {
            // free(list);
            return NULL;
        }
        list->head->next = list->head;
        list->head->prev = list->head;
        list->head->data = NULL;
        list->size = 0;
    }
    return list;
}

// destroy the list
void ylist_destroy(ylist *list)
{
    if (list)
    {
        void *data = NULL;
        do
        {
            data = ylist_pop_front(list);
        } while (data);
        // free(list->head);
        free(list);
    }
}

// destroy the list with free
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
        // free(list->head);
        free(list);
    }
}

// return iterator if success
ylist_iter *ylist_push_front(ylist *list, void *data)
{
    struct _ylist_iter *iter;
    if (!list || !data)
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

// return iterator if success
ylist_iter *ylist_push_back(ylist *list, void *data)
{
    struct _ylist_iter *iter;
    if (!list || !data)
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

// pop out data of the first entry if success
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

// pop out data of the last entry if success
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

// true if it is empty.
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

// return the data of the first entry
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

// return the data of the last entry
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

ylist_iter *ylist_first(ylist *list)
{
    if (!list)
        return NULL;
    if (list->head->next == list->head)
        return NULL;
    return list->head->next;
}

ylist_iter *ylist_last(ylist *list)
{
    if (!list)
        return NULL;
    if (list->head->prev == list->head)
        return NULL;
    return list->head->prev;
}

// return next iterator of the current iterator.
ylist_iter *ylist_next(ylist_iter *iter)
{
    iter = iter ? (iter->next) : NULL;
    if (iter && iter->data)
        return iter;
    return NULL;
}

// return previous iterator of the current iterator.
ylist_iter *ylist_prev(ylist_iter *iter)
{
    iter = iter ? (iter->prev) : NULL;
    if (iter && iter->data)
        return iter;
    return NULL;
}

// return Xth iterator in the list.
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
        iter = ylist_next(iter);
        count++;
    }
    return iter;
}

// true if the iterator ended
int ylist_done(ylist_iter *iter)
{
    if (!iter)
        return 1;
    if (iter->data)
        return 0;
    return 1;
}

// get data of the iterator
void *ylist_data(ylist_iter *iter)
{
    if (iter)
    {
        return iter->data;
    }
    return NULL;
}

// delete the data of the iterator and then return prev iterator.
ylist_iter *ylist_erase(ylist_iter *iter, user_free ufree)
{
    if (!iter)
        return NULL;
    if (iter->data)
    {
        struct _ylist_iter *prev;
        prev = iter->prev;
        if (ufree)
            ufree(iter->data);
        iter->prev->next = iter->next;
        iter->next->prev = iter->prev;
        free(iter);
        iter = prev->next;
        while (iter && iter->data) {
            iter = iter->next;
        }
        if (iter && iter->data == NULL)
        { // head
            ylist *list = (ylist *) iter;
            list->size--;
        }
        if (prev->data == NULL) // head
            return NULL;
        return prev;
    }
    else
    {
        return NULL;
    }
}

// insert the data the next of the iterator and then return new iterator for the data
// return NULL if it failed.
ylist_iter *ylist_insert(ylist *list, ylist_iter *iter, void *data)
{
    struct _ylist_iter *new_iter;
    if (!data || (!list && !iter))
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
    
        if (list)
            list->size++;
        else
        {
            iter = new_iter->next;
            while (iter && iter->data) {
                iter = iter->next;
            }
            if (iter && iter->data == NULL)
            { // head
                ylist *list = (ylist *) iter;
                list->size++;
            }
        }
    }
    return new_iter;
}

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

