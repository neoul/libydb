
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ytree.h"

// https://rosettacode.org/wiki/AVL_tree/C
// Modified by neoul with additional functions.

//
// Private datatypes
//

typedef struct _ytree *Tree;
typedef struct _ytree_node *Node;
struct _ytree
{
    Node root;
    ytree_cmp comp;
    user_free key_free;
    user_free data_free;
    ytree_print print;
    size_t size;
};

struct _ytree_node
{
    Node parent;
    Node left;
    Node right;
    void *key;
    void *data;
    int balance;
};

struct trunk
{
    struct trunk *prev;
    char *str;
};

//
// Declaration of private functions.
//
void Tree_InsertBalance(Tree t, Node node, int balance);
void Tree_DeleteBalance(Tree t, Node node, int balance);
Node Tree_RotateLeft(Tree t, Node node);
Node Tree_RotateRight(Tree t, Node node);
Node Tree_RotateLeftRight(Tree t, Node node);
Node Tree_RotateRightLeft(Tree t, Node node);
void Tree_Replace(Tree t, Node target, Node source);

Node Node_New(void *key, void *data, Node parent);

void print_tree(Tree t, Node n, struct trunk *prev, int is_left);

//----------------------------------------------------------------------------

// Tree_Insert --
//
//     Insert new key in the tree. If the key is already in the tree,
//     nothing will be done.
//
Node Tree_Insert(Tree t, void *key, void *data, int del, Node *_new)
{
    if (t == NULL)
        return NULL;
    if (t->root == NULL)
    {
        t->root = Node_New(key, data, NULL);
        if (_new)
            *_new = t->root;
        t->size++;
        return NULL;
    }
    else
    {
        Node node = t->root;
        while (node != NULL)
        {
            if ((t->comp)(key, node->key) < 0)
            {
                Node left = node->left;
                if (left == NULL)
                {
                    node->left = Node_New(key, data, node);
                    if (_new)
                        *_new = node->left;
                    Tree_InsertBalance(t, node, -1);
                    t->size++;
                    return NULL;
                }
                else
                {
                    node = left;
                }
            }
            else if ((t->comp)(key, node->key) > 0)
            {
                Node right = node->right;
                if (right == NULL)
                {
                    node->right = Node_New(key, data, node);
                    if (_new)
                        *_new = node->right;
                    Tree_InsertBalance(t, node, 1);
                    t->size++;
                    return NULL;
                }
                else
                {
                    node = right;
                }
            }
            else
            {
                if (del)
                {
                    if (t->key_free && node->key)
                        t->key_free(node->key);
                    if (t->data_free && node->data)
                        t->data_free(node->data);
                    node->key = key;
                    node->data = data;
                    return NULL;
                }
                return node;
            }
        }
        // shouldn't be reach here.
        printf("avl - reached unexpected condition.\n");
        assert(0);
        return NULL;
    }
}

// Tree_DeleteNode --
//
//     Removes a given node from the tree.
//
void Tree_DeleteNode(Tree t, Node node)
{
    if (t == NULL)
        return;
    Node left = node->left;
    Node right = node->right;
    Node toDelete = node;

    if (left == NULL)
    {
        if (right == NULL)
        {
            if (node == t->root)
            {
                t->root = NULL;
            }
            else
            {
                Node parent = node->parent;
                if (parent->left == node)
                {
                    parent->left = NULL;
                    Tree_DeleteBalance(t, parent, 1);
                }
                else
                {
                    parent->right = NULL;
                    Tree_DeleteBalance(t, parent, -1);
                }
            }
        }
        else
        {
            Tree_Replace(t, node, right);
            Tree_DeleteBalance(t, right, 0);
        }
    }
    else if (right == NULL)
    {
        Tree_Replace(t, node, left);
        Tree_DeleteBalance(t, left, 0);
    }
    else
    {
        Node successor = right;
        if (successor->left == NULL)
        {
            Node parent = node->parent;
            successor->parent = parent;
            successor->left = left;
            successor->balance = node->balance;

            if (left != NULL)
            {
                left->parent = successor;
            }
            if (node == t->root)
            {
                t->root = successor;
            }
            else
            {
                if (parent->left == node)
                {
                    parent->left = successor;
                }
                else
                {
                    parent->right = successor;
                }
            }
            Tree_DeleteBalance(t, successor, -1);
        }
        else
        {
            while (successor->left != NULL)
            {
                successor = successor->left;
            }
            Node parent = node->parent;
            Node successorParent = successor->parent;
            Node successorRight = successor->right;

            if (successorParent->left == successor)
            {
                successorParent->left = successorRight;
            }
            else
            {
                successorParent->right = successorRight;
            }

            if (successorRight != NULL)
            {
                successorRight->parent = successorParent;
            }

            successor->parent = parent;
            successor->left = left;
            successor->balance = node->balance;
            successor->right = right;
            right->parent = successor;

            if (left != NULL)
            {
                left->parent = successor;
            }

            if (node == t->root)
            {
                t->root = successor;
            }
            else
            {
                if (parent->left == node)
                {
                    parent->left = successor;
                }
                else
                {
                    parent->right = successor;
                }
            }
            Tree_DeleteBalance(t, successorParent, 1);
        }
    }

    free(toDelete);
    t->size--;
}

// Tree_SearchNode --
//
//     Searches the tree for a node containing the given key.
//
Node Tree_SearchNode(Tree t, void *key)
{
    Node node;
    if (t == NULL)
        return NULL;
    node = t->root;
    while (node != NULL)
    {
        if ((t->comp)(key, node->key) < 0)
        {
            node = node->left;
        }
        else if ((t->comp)(key, node->key) > 0)
        {
            node = node->right;
        }
        else
        {
            return node;
        }
    }

    return NULL;
}

// Tree_Print --
//
//     Prints an ASCII representation of the tree on screen.
//
void Tree_Print(Tree t)
{
    print_tree(t, t->root, 0, 0);
    fflush(stdout);
}

// Tree_TopNode --
//
//     Returns the top node.
//
Node Tree_TopNode(Tree t)
{
    Node node = t ? t->root : NULL;
    return node;
}

// Tree_FirstNode --
//
//     Returns the node containing the smallest key.
//
Node Tree_FirstNode(Tree t)
{
    Node node = t ? t->root : NULL;

    while ((node != NULL) && (node->left != NULL))
    {
        node = node->left;
    }

    return node;
}

// Tree_LastNode --
//
//     Returns the node containing the biggest key.
//
Node Tree_LastNode(Tree t)
{
    Node node = t ? t->root : NULL;

    while ((node != NULL) && (node->right != NULL))
    {
        node = node->right;
    }

    return node;
}

// Tree_PrevNode --
//
//     Returns the predecessor of the given node.
//
Node Tree_PrevNode(Tree t, Node n)
{
    Node nTemp;
    if (t == NULL)
        return NULL;

    if (n->left != NULL)
    {
        n = n->left;
        while (n->right != NULL)
        {
            n = n->right;
        }
    }
    else
    {
        nTemp = n;
        n = n->parent;
        while ((n != NULL) && (n->left == nTemp))
        {
            nTemp = n;
            n = n->parent;
        }
    }
    return n;
}

// Tree_NextNode --
//
//     Returns the follower of the given node.
//
Node Tree_NextNode(Tree t, Node n)
{
    Node nTemp;
    if (t == NULL)
        return NULL;

    if (n->right != NULL)
    {
        n = n->right;
        while (n->left != NULL)
        {
            n = n->left;
        }
    }
    else
    {
        nTemp = n;
        n = n->parent;
        while ((n != NULL) && (n->right == nTemp))
        {
            nTemp = n;
            n = n->parent;
        }
    }

    return n;
}

//----------------------------------------------------------------------------
//
// Internal functions.
//

void Tree_InsertBalance(Tree t, Node node, int balance)
{
    while (node != NULL)
    {
        balance = (node->balance += balance);
        if (balance == 0)
        {
            return;
        }
        else if (balance == -2)
        {
            if (node->left->balance == -1)
            {
                Tree_RotateRight(t, node);
            }
            else
            {
                Tree_RotateLeftRight(t, node);
            }
            return;
        }
        else if (balance == 2)
        {
            if (node->right->balance == 1)
            {
                Tree_RotateLeft(t, node);
            }
            else
            {
                Tree_RotateRightLeft(t, node);
            }
            return;
        }
        Node parent = node->parent;
        if (parent != NULL)
        {
            balance = (parent->left == node) ? -1 : 1;
        }
        node = parent;
    }
}

void Tree_DeleteBalance(Tree t, Node node, int balance)
{
    while (node != NULL)
    {
        balance = (node->balance += balance);

        if (balance == -2)
        {
            if (node->left->balance <= 0)
            {
                node = Tree_RotateRight(t, node);

                if (node->balance == 1)
                {
                    return;
                }
            }
            else
            {
                node = Tree_RotateLeftRight(t, node);
            }
        }
        else if (balance == 2)
        {
            if (node->right->balance >= 0)
            {
                node = Tree_RotateLeft(t, node);

                if (node->balance == -1)
                {
                    return;
                }
            }
            else
            {
                node = Tree_RotateRightLeft(t, node);
            }
        }
        else if (balance != 0)
        {
            return;
        }

        Node parent = node->parent;

        if (parent != NULL)
        {
            balance = (parent->left == node) ? 1 : -1;
        }

        node = parent;
    }
}

void Tree_Replace(Tree t, Node target, Node source)
{
	Node parent = target->parent;
	
	if (parent)
	{
		if (parent->left == target)
			parent->left = source;
		else if (parent->right == target)
			parent->right = source;
		source->parent = parent;
	}
	else
	{
		t->root = source;
		source->parent = NULL;
	}
}

Node Tree_RotateLeft(Tree t, Node node)
{
    Node right = node->right;
    Node rightLeft = right->left;
    Node parent = node->parent;

    right->parent = parent;
    right->left = node;
    node->right = rightLeft;
    node->parent = right;

    if (rightLeft != NULL)
    {
        rightLeft->parent = node;
    }

    if (node == t->root)
    {
        t->root = right;
    }
    else if (parent->right == node)
    {
        parent->right = right;
    }
    else
    {
        parent->left = right;
    }

    right->balance--;
    node->balance = -right->balance;

    return right;
}

Node Tree_RotateRight(Tree t, Node node)
{
    Node left = node->left;
    Node leftRight = left->right;
    Node parent = node->parent;

    left->parent = parent;
    left->right = node;
    node->left = leftRight;
    node->parent = left;

    if (leftRight != NULL)
    {
        leftRight->parent = node;
    }

    if (node == t->root)
    {
        t->root = left;
    }
    else if (parent->left == node)
    {
        parent->left = left;
    }
    else
    {
        parent->right = left;
    }

    left->balance++;
    node->balance = -left->balance;

    return left;
}

Node Tree_RotateLeftRight(Tree t, Node node)
{
    Node left = node->left;
    Node leftRight = left->right;
    Node parent = node->parent;
    Node leftRightRight = leftRight->right;
    Node leftRightLeft = leftRight->left;

    leftRight->parent = parent;
    node->left = leftRightRight;
    left->right = leftRightLeft;
    leftRight->left = left;
    leftRight->right = node;
    left->parent = leftRight;
    node->parent = leftRight;

    if (leftRightRight != NULL)
    {
        leftRightRight->parent = node;
    }

    if (leftRightLeft != NULL)
    {
        leftRightLeft->parent = left;
    }

    if (node == t->root)
    {
        t->root = leftRight;
    }
    else if (parent->left == node)
    {
        parent->left = leftRight;
    }
    else
    {
        parent->right = leftRight;
    }

    if (leftRight->balance == 1)
    {
        node->balance = 0;
        left->balance = -1;
    }
    else if (leftRight->balance == 0)
    {
        node->balance = 0;
        left->balance = 0;
    }
    else
    {
        node->balance = 1;
        left->balance = 0;
    }

    leftRight->balance = 0;

    return leftRight;
}

Node Tree_RotateRightLeft(Tree t, Node node)
{
    Node right = node->right;
    Node rightLeft = right->left;
    Node parent = node->parent;
    Node rightLeftLeft = rightLeft->left;
    Node rightLeftRight = rightLeft->right;

    rightLeft->parent = parent;
    node->right = rightLeftLeft;
    right->left = rightLeftRight;
    rightLeft->right = right;
    rightLeft->left = node;
    right->parent = rightLeft;
    node->parent = rightLeft;

    if (rightLeftLeft != NULL)
    {
        rightLeftLeft->parent = node;
    }

    if (rightLeftRight != NULL)
    {
        rightLeftRight->parent = right;
    }

    if (node == t->root)
    {
        t->root = rightLeft;
    }
    else if (parent->right == node)
    {
        parent->right = rightLeft;
    }
    else
    {
        parent->left = rightLeft;
    }

    if (rightLeft->balance == -1)
    {
        node->balance = 0;
        right->balance = 1;
    }
    else if (rightLeft->balance == 0)
    {
        node->balance = 0;
        right->balance = 0;
    }
    else
    {
        node->balance = -1;
        right->balance = 0;
    }

    rightLeft->balance = 0;

    return rightLeft;
}

void print_trunks(struct trunk *p)
{
    if (!p)
    {
        return;
    }
    print_trunks(p->prev);
    printf("%s", p->str);
}

void print_tree(Tree t, Node n, struct trunk *prev, int is_left)
{
    if (t == NULL)
        return;
    if (n == NULL)
    {
        return;
    }

    struct trunk this_disp = {prev, "     "};
    char *prev_str = this_disp.str;
    print_tree(t, n->right, &this_disp, 1);

    if (!prev)
    {
        this_disp.str = "---";
    }
    else if (is_left)
    {
        this_disp.str = ".--";
        prev_str = "    |";
    }
    else
    {
        this_disp.str = "`--";
        prev->str = prev_str;
    }

    print_trunks(&this_disp);
    (t->print)(n->key);
    printf(" (%+d)\n", n->balance);

    if (prev)
    {
        prev->str = prev_str;
    }
    this_disp.str = "    |";

    print_tree(t, n->left, &this_disp, 0);
    if (!prev)
    {
        puts("");
    }
}

Node Node_New(void *key, void *data, Node parent)
{
    Node n;
    n = malloc(sizeof(*n));
    n->parent = parent;
    n->left = NULL;
    n->right = NULL;
    n->key = key;
    n->data = data;
    n->balance = 0;
    return n;
}

void default_print(void *a)
{
    printf("%p", a);
}

int default_cmp(void *a1, void *a2)
{
    if (a1 < a2)
        return -1;
    else if (a1 > a2)
        return 1;
    else
        return 0;
}

// create ytree with compare, key and data free functions.
ytree *ytree_create(ytree_cmp comp, user_free key_free)
{
    struct _ytree *tree = NULL;
    if (!comp)
        comp = default_cmp;

    tree = malloc(sizeof(struct _ytree));
    if (tree)
    {
        memset((void *)tree, 0x0, sizeof(struct _ytree));
        tree->comp = comp;
        tree->key_free = key_free;
        tree->data_free = NULL;
        tree->print = default_print;
    }
    return tree;
}

// destroy the tree with deleting all entries.
void ytree_destroy_custom(ytree *tree, user_free data_free)
{
    if (!tree)
        return;
    Node node = Tree_TopNode(tree);
    while (node != NULL)
    {
        void *key = node->key;
        void *data = node->data;
        Tree_DeleteNode(tree, node);
        if (tree->key_free && key)
            tree->key_free(key);
        if (data_free && data)
            data_free(data);
        else if (tree->data_free && data)
            tree->data_free(data);
        node = Tree_TopNode(tree);
    }
    free(tree);
}

void ytree_destroy(ytree *tree)
{
    ytree_destroy_custom(tree, NULL);
}

unsigned int ytree_size(ytree *tree)
{
    return tree->size;
}

// return NULL if ok, otherwise return old data.
void *ytree_insert(ytree *tree, void *key, void *data)
{
    Node node = Tree_Insert(tree, key, data, 0, NULL);
    if (node)
    {
        void *rdata = node->data;
        if (tree->key_free && node->key)
            tree->key_free(node->key);
        rdata = node->data;
        node->key = key;
        node->data = data;
        return rdata;
    }
    return NULL;
}

// insert the key and data without failure.
// data_free is used to remove old data.
void ytree_insert_custom(ytree *tree, void *key, void *data, user_free data_free)
{
    user_free temp_free;
    if (!tree)
        return;
    temp_free = tree->data_free;
    tree->data_free = data_free;
    Tree_Insert(tree, key, data, 1, NULL);
    tree->data_free = temp_free;
}

// delete the key and then return the found data, otherwise return NULL
void *ytree_delete(ytree *tree, void *key)
{
    Node node = Tree_SearchNode(tree, key);
    if (node)
    {
        void *rkey = node->key;
        void *rdata = node->data;
        Tree_DeleteNode(tree, node);
        if (tree->key_free && rkey)
            tree->key_free(rkey);
        return rdata;
    }
    return NULL;
}

// delete data using user_free
void ytree_delete_custom(ytree *tree, void *key, user_free data_free)
{
    Node node = Tree_SearchNode(tree, key);
    if (node)
    {
        void *rkey = node->key;
        void *rdata = node->data;
        Tree_DeleteNode(tree, node);
        if (tree->key_free && rkey)
            tree->key_free(rkey);
        if (data_free && rdata)
            data_free(rdata);
    }
    return;
}

// return the value if found, otherwise return NULL
void *ytree_search(ytree *tree, void *key)
{
    Node node = Tree_SearchNode(tree, key);
    if (node)
        return node->data;
    return NULL;
}

// return the value if found, otherwise return NULL
int ytree_exist(ytree *tree, void *key)
{
    Node node = Tree_SearchNode(tree, key);
    if (node)
        return 1;
    return 0;
}

// Iterates through entries in the tree
int ytree_traverse(ytree *tree, ytree_callback cb, void *user_data)
{
    Node node = Tree_FirstNode(tree);
    while (node != NULL)
    {
        void *key = node->key;
        void *data = node->data;
        int res = cb(key, data, user_data);
        if (res != 0)
            return res;
        node = Tree_NextNode(tree, node);
    }
    return 0;
}

// Iterates through entries in the tree in reverse direction
int ytree_traverse_reverse(ytree *tree, ytree_callback cb, void *user_data)
{
    Node node = Tree_LastNode(tree);
    while (node != NULL)
    {
        void *key = node->key;
        void *data = node->data;
        int res = cb(key, data, user_data);
        if (res != 0)
            return res;
        node = Tree_PrevNode(tree, node);
    }
    return 0;
}

// Iterates through the entries in the tree within a range.
int ytree_traverse_in_range(ytree *tree, void *lower_boundary, void *higher_boundary, ytree_callback cb, void *user_data)
{
    int res;
    void *key;
    void *data;
    ytree_cmp cmp;
    Node base;
    Node node;

    cmp = tree->comp;
    base = ytree_find_nearest(tree, lower_boundary);
    if (!cmp)
        return -1;
    if (!base)
        return -1;
    node = base;
    while (node != NULL)
    {
        key = node->key;
        data = node->data;
        res = cmp(lower_boundary, key);
        if (res > 0)
            break;

        res = cb(key, data, user_data);
        if (res != 0)
            return res;
        node = Tree_PrevNode(tree, node);
    }
    node = Tree_NextNode(tree, base);
    while (node != NULL)
    {
        key = node->key;
        data = node->data;
        res = cmp(key, higher_boundary);
        if (res > 0)
            break;
        res = cb(key, data, user_data);
        if (res != 0)
            return res;
        node = Tree_NextNode(tree, node);
    }
    return 0;
}

ytree_iter *ytree_find_nearest(ytree *tree, void *key)
{
    ytree_iter *node = tree->root;
    ytree_iter *nearest = node;
    while (node != NULL)
    {
        if ((tree->comp)(key, node->key) < 0)
        {
            nearest = node;
            node = node->left;
        }
        else if ((tree->comp)(key, node->key) > 0)
        {
            nearest = node;
            node = node->right;
        }
        else
        {
            return node;
        }
    }
    return nearest;
}

// return the value if found, otherwise return NULL
ytree_iter *ytree_find(ytree *tree, void *key)
{
    return Tree_SearchNode(tree, key);
}

ytree_iter *ytree_top(ytree *tree)
{
    return Tree_TopNode(tree);
}

ytree_iter *ytree_first(ytree *tree)
{
    return Tree_FirstNode(tree);
}

ytree_iter *ytree_last(ytree *tree)
{
    return Tree_LastNode(tree);
}

ytree_iter *ytree_prev(ytree *tree, ytree_iter *n)
{
    return Tree_PrevNode(tree, n);
}

ytree_iter *ytree_next(ytree *tree, ytree_iter *n)
{
    return Tree_NextNode(tree, n);
}

// ytree_push --
//
//     Insert new key to the tree and return the iterator.
//     If the key exists in the tree, return old_data.
//
ytree_iter *ytree_push(ytree *tree, void *key, void *data, void **old_data)
{
    Node _new = NULL;
    Node node = Tree_Insert(tree, key, data, 0, &_new);
    if (node)
    {
        void *rdata = node->data;
        if (tree->key_free && node->key)
            tree->key_free(node->key);
        rdata = node->data;
        node->key = key;
        node->data = data;
        if (old_data)
            *old_data = rdata;
        return node;
    }
    return _new;
}

// ytree_remove --
//
//     Remove the target iterator from the tree and return the data.
//
void *ytree_remove(ytree *tree, ytree_iter *n)
{
    void *data = n->data;
    if (tree->key_free && n->key)
        tree->key_free(n->key);
    Tree_DeleteNode(tree, n);
    return data;
}

void *ytree_data(ytree_iter *n)
{
    if (n)
        return n->data;
    return NULL;
}

void *ytree_key(ytree_iter *n)
{
    if (n)
        return n->key;
    return NULL;
}
