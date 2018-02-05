
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avl.h"
#include "ytree.h"

void default_print(void *a) {
    printf ("%p", a);
}

int default_cmp (void *a1, void *a2) {
    if(a1 < a2)
        return -1;
    else if(a1 > a2)
        return 1;
    else
        return 0;
}

ytree ytree_create(ytree_cmp cmp, ytree_print print)
{
    Tree tree;
    if(!cmp)
        cmp = default_cmp;
    if(!print)
        print = default_print;
    
    tree = Tree_New (cmp, print);
    return (ytree) tree;
}

void ytree_destroy(ytree tree)
{
    if(tree)
        free(tree);
}


void ytree_destroy_custom(ytree tree, user_free data_free)
{
    if(!tree || !data_free)
        return;
    Node node = Tree_TopNode(tree);
    while(node != NULL)
    {
        void *data = Node_GetData(node);
        Tree_DeleteNode(tree, node);
        if(data_free)
            data_free(data);
        node = Tree_TopNode(tree);
    }
    free(tree);
}

unsigned int ytree_size(ytree tree)
{
    return Tree_Size(tree);
}

// return NULL if ok, otherwise return old value
void *ytree_insert(ytree tree, void *data)
{
    return Tree_Insert(tree, data);
}

// delete the data and then return the found data, otherwise return NULL
void *ytree_delete(ytree tree, void *data)
{
    Node node = Tree_SearchNode(tree, data);
    if (node)
    {
        void *rdata = Node_GetData(node);
        Tree_DeleteNode(tree, node);
        return rdata;
    }
    return NULL;
}

// return the value if found, otherwise return NULL
void *ytree_search(ytree tree, void *data)
{
    Node node = Tree_SearchNode(tree, data);
    if (node)
    {
        return Node_GetData(node);
    }
    return NULL;
}

// Iterates through entries in the tree
int ytree_traverse(ytree tree, ytree_callback cb, void *user_data)
{
    Node node = Tree_FirstNode(tree);
    while(node != NULL)
    {
        void *data = Node_GetData(node);
        int res = cb(data, user_data);
        if(res != 0)
            return res;
        node = Tree_NextNode(tree, node);
    }
}

// Iterates through entries in the tree in reverse direction
int ytree_traverse_reverse(ytree tree, ytree_callback cb, void *user_data)
{
    Node node = Tree_LastNode(tree);
    while(node != NULL)
    {
        void *data = Node_GetData(node);
        int res = cb(data, user_data);
        if(res != 0)
            return res;
        node = Tree_PrevNode(tree, node);
    }
}

// Iterates through the entries in the tree within a range.
int ytree_traverse_in_range(ytree tree, void *lower_boundary_data, void *higher_boundary_data, ytree_callback cb, void *user_data)
{
    int res;
    void *data;
    ytree_cmp cmp;
    Node base;
    Node node;
    struct Tree *t = tree;

    cmp = Tree_GetComp(tree);
    base = Tree_SearchNode_Nearest(tree, lower_boundary_data);
    if(!cmp)
        return -1;
    if(!base)
        return -1;
    node = base;
    while(node != NULL)
    {
        data = Node_GetData(node);
        res = cmp(lower_boundary_data, data);
        if(res > 0)
            break;

        res = cb(data, user_data);
        if(res != 0)
            return res;
        node = Tree_PrevNode(tree, node);
    }
    node = Tree_NextNode(tree, base);
    while(node != NULL)
    {
        data = Node_GetData(node);
        res = cmp(data, higher_boundary_data);
        if(res > 0)
            break;
        res = cb(data, user_data);
        if(res != 0)
            return res;
        node = Tree_NextNode(tree, node);
    }
    return 0;
}

void ytree_printf(ytree tree)
{
    Tree_Print(tree);
}

ynode ytree_top(ytree tree)
{
    return Tree_TopNode(tree);
}

ynode ytree_first(ytree tree)
{
    return Tree_FirstNode(tree);
}

ynode ytree_last(ytree tree)
{
    return Tree_LastNode(tree);
}
 
ynode  ytree_prev(ytree tree, ynode n)
{
    return Tree_PrevNode(tree, n);
}

ynode  ytree_next(ytree tree, ynode n)
{
    return Tree_NextNode(tree, n);
}

void *ytree_data(ynode n)
{
    return Node_GetData(n);
}
