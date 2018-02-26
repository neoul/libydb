
#include <stdio.h>
#include <stdlib.h>
#include "ylist.h"

struct node
{
    struct node *next;
    struct node *prev;
    void *data;
};

struct list
{
    struct node *head;
    ylist_cmp comp;
};

// create a list
ylist ylist_create()
{
    struct list *l = malloc(sizeof(struct list));
    if (l)
    {
        l->head = malloc(sizeof(struct node));
        if (!l->head)
        {
            free(l);
            return NULL;
        }
        l->head->next = l->head;
        l->head->prev = l->head;
        l->head->data = NULL;
        l->comp = NULL;
    }
    return l;
}

// destroy the list
void ylist_destroy(ylist list)
{
    struct list *l = list;
    if (l)
    {
        void *data = NULL;
        do
        {
            data = ylist_pop_front(list);
        } while (data);
        free(l->head);
        free(l);
    }
}

// destroy the list with free
void ylist_destroy_custom(ylist list, user_free ufree)
{
    struct list *l = list;
    if (l)
    {
        void *data = NULL;
        do
        {
            data = ylist_pop_front(list);
            if (data && ufree)
                ufree(data);
        } while (data);
        free(l->head);
        free(l);
    }
}

// return ylist_iter if success
ylist_iter ylist_push_front(ylist list, void *data)
{
    struct list *l;
    struct node *n;
    if (!list || !data)
        return NULL;
    l = list;
    n = malloc(sizeof(struct node));
    if (n)
    {
        n->data = data;
        n->next = l->head->next;
        n->prev = l->head;
        l->head->next->prev = n;
        l->head->next = n;
    }
    return n;
}

// return ylist_iter if success
ylist_iter ylist_push_back(ylist list, void *data)
{
    struct list *l;
    struct node *n;
    if (!list || !data)
        return NULL;
    l = list;
    n = malloc(sizeof(struct node));
    if (n)
    {
        n->data = data;
        n->next = l->head;
        n->prev = l->head->prev;
        l->head->prev->next = n;
        l->head->prev = n;
    }
    return n;
}

// pop out data of the first entry if success
void *ylist_pop_front(ylist list)
{
    struct list *l;
    struct node *n;
    if (!list)
        return NULL;
    l = list;
    n = l->head->next;
    if (n == l->head)
        return NULL;
    else
    {
        void *data = n->data;
        n->next->prev = l->head;
        l->head->next = n->next;
        free(n);
        return data;
    }
}

// pop out data of the last entry if success
void *ylist_pop_back(ylist list)
{
    struct list *l;
    struct node *n;
    if (!list)
        return NULL;
    l = list;
    n = l->head->prev;
    if (n == l->head)
        return NULL;
    else
    {
        void *data = n->data;
        n->prev->next = l->head;
        l->head->prev = n->prev;
        free(n);
        return data;
    }
}

// true if it is empty.
int ylist_empty(ylist list)
{
    if (list)
    {
        struct list *l = list;
        if (l->head->next == l->head)
            return 1;
        if (l->head->prev == l->head)
            return 1;
        return 0;
    }
    return 1;
}

// return the data of the first entry
void *ylist_front(ylist list)
{
    struct list *l;
    struct node *n;
    if (!list)
        return NULL;
    l = list;
    n = l->head->next;
    if (n == l->head)
        return NULL;
    else
        return n->data;
}

// return the data of the last entry
void *ylist_back(ylist list)
{
    struct list *l;
    struct node *n;
    if (!list)
        return NULL;
    l = list;
    n = l->head->prev;
    if (n == l->head)
        return NULL;
    else
        return n->data;
}

ylist_iter ylist_iter_front(ylist list)
{
    struct list *l;
    if (!list)
        return NULL;
    l = list;
    return l->head->next;
}

ylist_iter ylist_iter_back(ylist list)
{
    struct list *l;
    if (!list)
        return NULL;
    l = list;
    return l->head->prev;
}

// return next iterator of the current iterator.
ylist_iter ylist_iter_next(ylist_iter iter)
{
    return iter ? (((struct node *)(iter))->next) : NULL;
}

// return previous iterator of the current iterator.
ylist_iter ylist_iter_prev(ylist_iter iter)
{
    return iter ? (((struct node *)(iter))->prev) : NULL;
}

// true if the iterator ended
int ylist_iter_done(ylist_iter iter)
{
    if (!iter)
        return 1;
    if (((struct node *)(iter))->data)
        return 0;
    return 1;
}

// get data of the iterator
void *ylist_iter_data(ylist_iter iter)
{
    struct node *n;
    if (iter)
    {
        n = iter;
        return n->data;
    }
    return NULL;
}

// delete the data of the iterator and then return next iterator.
ylist_iter ylist_erase(ylist_iter iter, user_free ufree)
{
    struct node *n;
    if (!iter)
        return iter;
    n = iter;
    if (n->data)
    {
        struct node *next;
        if (ufree)
            ufree(n->data);
        n->prev->next = n->next;
        n->next->prev = n->prev;
        next = n->next;
        free(n);
        return next;
    }
    else
    {
        return iter;
    }
}

// insert the data ahead of the iterator and then return new iterator for the data
// return NULL if it failed.
ylist_iter ylist_insert(ylist_iter iter, void *data)
{
    struct node *n, *inode;
    if (!iter || !data)
        return NULL;
    n = iter;
    inode = malloc(sizeof(struct node));
    if (inode)
    {
        inode->data = data;
        inode->next = n;
        inode->prev = n->prev;
        n->prev->next = inode;
        n->prev = inode;
    }
    return inode;
}

void ylist_printf(ylist list, ylist_print print, void *addition)
{
    struct list *l;
    struct node *n;
    if (!list)
        return;
    l = list;

    for (n = l->head->next; n != l->head; n = n->next)
    {
        print(n->data, addition);
    }
}

void ylist_traverse(ylist list, ylist_callback cb, void *addition)
{
    struct list *l;
    struct node *n;
    if (!list)
        return;
    l = list;

    for (n = l->head->next; n != l->head; n = n->next)
    {
        cb(n->data, addition);
    }
}

