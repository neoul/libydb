// https://rosettacode.org/wiki/AVL_tree/C
// Edited by neoul

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "avl.h"
 
//
// Private datatypes
//
struct Tree {
    Node root;
    int (*comp) (void *, void *);
    void (*print) (void *);
    size_t size;
};
 
struct Node {
    Node parent;
    Node left;
    Node right;
    void *data;
    int balance;
};
 
struct trunk {
    struct trunk *prev;
    char *str;
};
 
//
// Declaration of private functions.
//
void Tree_InsertBalance (Tree t, Node node, int balance);
void Tree_DeleteBalance (Tree t, Node node, int balance);
Node Tree_RotateLeft (Tree t, Node node);
Node Tree_RotateRight (Tree t, Node node);
Node Tree_RotateLeftRight (Tree t, Node node);
Node Tree_RotateRightLeft (Tree t, Node node);
void Tree_Replace (Node target, Node source);
 
Node Node_New (void *data, Node parent);
 
void print_tree (Tree t, Node n, struct trunk *prev, int is_left);
 
//----------------------------------------------------------------------------
 
// Tree_New --
//
//     Creates a new tree using the parameters 'comp' and 'print' for
//     comparing and printing the data in the nodes.
//
Tree Tree_New (int (*comp)(void *, void *), void (*print)(void *)) {
    Tree t;
 
    t = malloc (sizeof (*t));
    memset((void *)t, 0x0, sizeof(*t));
    t->comp = comp;
    t->print = print;
    t->size = 0;
    return t;
}

void *Tree_GetComp(Tree t)
{
    return t->comp;
}
 
// Tree_Insert --
//
//     Insert new data in the tree. If the data is already in the tree,
//     nothing will be done.
//
void *Tree_Insert (Tree t, void *data) {
    if (t->root == NULL) {
        t->root = Node_New (data, NULL);
        t->size++;
        return NULL;
    } else {
        Node node = t->root;
        while (node != NULL) {
            if ((t->comp) (data, node->data) < 0) {
                Node left = node->left;
                if (left == NULL) {
                    node->left = Node_New (data, node);
                    Tree_InsertBalance (t, node, -1);
                    t->size++;
                    return NULL;
                } else {
                    node = left;
                }
            } else if ((t->comp) (data, node->data) > 0) {
                Node right = node->right;
                if (right == NULL) {
                    node->right = Node_New (data, node);
                    Tree_InsertBalance (t, node, 1);
                    t->size++;
                    return NULL;
                } else {
                    node = right;
                }
            } else {
                void *old = node->data;
                node->data = data;
                return old;
            }
        }
        // shouldn't be reach here.
        printf("avl - reached unexpected condition.\n");
        return NULL;
    }
}
 
// Tree_DeleteNode --
//
//     Removes a given node from the tree.
//
void Tree_DeleteNode (Tree t, Node node) {
    Node left = node->left;
    Node right = node->right;
    Node toDelete = node;
 
    if (left == NULL) {
        if (right == NULL) {
            if (node == t->root) {
                t->root = NULL;
            } else {
                Node parent = node->parent;
                if (parent->left == node) {
                    parent->left = NULL;
                    Tree_DeleteBalance (t, parent, 1);
                } else {
                    parent->right = NULL;
                    Tree_DeleteBalance (t, parent, -1);
                }
            }
        } else {
            Tree_Replace (node, right);
            Tree_DeleteBalance (t, node, 0);
            toDelete = right;
        }
    } else if (right == NULL) {
        Tree_Replace (node, left);
        Tree_DeleteBalance (t, node, 0);
        toDelete = left;
    } else {
        Node successor = right;
        if (successor->left == NULL) {
            Node parent = node->parent;
            successor->parent = parent;
            successor->left = left;
            successor->balance = node->balance;
 
            if (left != NULL) {
                left->parent = successor;
            }
            if (node == t->root) {
                t->root = successor;
            } else {
                if (parent->left == node) {
                    parent->left = successor;
                } else {
                    parent->right = successor;
                }
            }
            Tree_DeleteBalance (t, successor, -1);
        } else {
            while (successor->left != NULL) {
                successor = successor->left;
            }
            Node parent = node->parent;
            Node successorParent = successor->parent;
            Node successorRight = successor->right;
 
            if (successorParent->left == successor) {
                successorParent->left = successorRight;
            } else {
                successorParent->right = successorRight;
            }
 
            if (successorRight != NULL) {
                successorRight->parent = successorParent;
            }
 
            successor->parent = parent;
            successor->left = left;
            successor->balance = node->balance;
            successor->right = right;
            right->parent = successor;
 
            if (left != NULL) {
                left->parent = successor;
            }
 
            if (node == t->root) {
                t->root = successor;
            } else {
                if (parent->left == node) {
                    parent->left = successor;
                } else {
                    parent->right = successor;
                }
            }
            Tree_DeleteBalance (t, successorParent, 1);
        }
    }
 
    free (toDelete);
    t->size--;
}
 
// Tree_SearchNode --
//
//     Searches the tree for a node containing the given data.
//
Node Tree_SearchNode (Tree t, void *data) {
    Node node = t->root;
 
    while (node != NULL) {
        if ((t->comp) (data, node->data) < 0) {
            node = node->left;
        } else if ((t->comp) (data, node->data) > 0) {
            node = node->right;
        } else {
            return node;
        }
    }
 
    return NULL;
}

// Tree_SearchNode_Nearest --
//
//     Searches the tree for a node containing the given data.
//
Node Tree_SearchNode_Nearest (Tree t, void *data) {
    Node node = t->root;
    Node nearest = node;
    
    while (node != NULL) {
        if ((t->comp) (data, node->data) < 0) {
            nearest = node;
            node = node->left;
        } else if ((t->comp) (data, node->data) > 0) {
            nearest = node;
            node = node->right;
        } else {
            return node;
        }
    }
 
    return nearest;
}
 
// Tree_Print --
//
//     Prints an ASCII representation of the tree on screen.
//
void Tree_Print (Tree t) {
    print_tree (t, t->root, 0, 0);
    fflush (stdout);
}


// Tree_TopNode --
//
//     Returns the top node.
//
Node Tree_TopNode (Tree t) {
    Node node = t->root;
    return node;
}
 
// Tree_FirstNode --
//
//     Returns the node containing the smallest key.
//
Node Tree_FirstNode (Tree t) {
    Node node = t->root;
 
    while ((node != NULL) && (node->left != NULL)) {
        node = node->left;
    }
 
    return node;
}
 
// Tree_LastNode --
//
//     Returns the node containing the biggest key.
//
Node Tree_LastNode (Tree t) {
    Node node = t->root;
 
    while ((node != NULL) && (node->right != NULL)) {
        node = node->right;
    }
 
    return node;
}
 
// Tree_PrevNode --
//
//     Returns the predecessor of the given node.
//
Node Tree_PrevNode (Tree t, Node n) {
    Node nTemp;
 
    if (n->left != NULL) {
        n = n->left;
        while (n->right != NULL) {
            n = n->right;
        }
    } else {
        nTemp = n;
        n = n->parent;
        while ((n != NULL) && (n->left == nTemp)) {
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
Node Tree_NextNode (Tree t, Node n) {
    Node nTemp;
 
    if (n->right != NULL) {
        n = n->right;
        while (n->left != NULL) {
            n = n->left;
        }
    } else {
        nTemp = n;
        n = n->parent;
        while ((n != NULL) && (n->right == nTemp)) {
            nTemp = n;
            n = n->parent;
        }
    }
 
    return n;
}
 
// Node_GetData --
//
//     Returns the data in a node.
//
void *Node_GetData (Node n) {
    return n->data;
}

// Tree_Size --
//
//     Returns the number of tree nodes.
//
unsigned int Tree_Size(Tree t) {
    return t->size;
}

//----------------------------------------------------------------------------
//
// Internal functions.
//
 
void Tree_InsertBalance (Tree t, Node node, int balance) {
    while (node != NULL) {
        balance = (node->balance += balance);
        if (balance == 0) {
            return;
        } else if (balance == -2) {
            if (node->left->balance == -1) {
                Tree_RotateRight (t, node);
            } else {
                Tree_RotateLeftRight (t, node);
            }
            return;
        } else if (balance == 2) {
            if (node->right->balance == 1) {
                Tree_RotateLeft (t, node);
            } else {
                Tree_RotateRightLeft (t, node);
            }
            return;
        }
        Node parent = node->parent;
        if (parent != NULL) {
            balance = (parent->left == node) ? -1 : 1;
        }
        node = parent;
    }
}
 
void Tree_DeleteBalance (Tree t, Node node, int balance) {
    while (node != NULL) {
        balance = (node->balance += balance);
 
        if (balance == -2) {
            if (node->left->balance <= 0) {
                node = Tree_RotateRight (t, node);
 
                if (node->balance == 1) {
                    return;
                }
            } else {
                node = Tree_RotateLeftRight (t, node);
            }
        } else if (balance == 2) {
            if (node->right->balance >= 0) {
                node = Tree_RotateLeft (t, node);
 
                if (node->balance == -1) {
                    return;
                }
            } else {
                node = Tree_RotateRightLeft (t, node);
            }
        } else if (balance != 0) {
            return;
        }
 
        Node parent = node->parent;
 
        if (parent != NULL) {
            balance = (parent->left == node) ? 1 : -1;
        }
 
        node = parent;
    }
}
 
void Tree_Replace (Node target, Node source) {
    Node left = source->left;
    Node right = source->right;
 
    target->balance = source->balance;
    target->data = source->data;
    target->left = left;
    target->right = right;
 
    if (left != NULL) {
        left->parent = target;
    }
 
    if (right != NULL) {
        right->parent = target;
    }
}
 
Node Tree_RotateLeft (Tree t, Node node) {
    Node right = node->right;
    Node rightLeft = right->left;
    Node parent = node->parent;
 
    right->parent = parent;
    right->left = node;
    node->right = rightLeft;
    node->parent = right;
 
    if (rightLeft != NULL) {
        rightLeft->parent = node;
    }
 
    if (node == t->root) {
        t->root = right;
    } else if (parent->right == node) {
        parent->right = right;
    } else {
        parent->left = right;
    }
 
    right->balance--;
    node->balance = -right->balance;
 
    return right;
}
 
Node Tree_RotateRight (Tree t, Node node) {
    Node left = node->left;
    Node leftRight = left->right;
    Node parent = node->parent;
 
    left->parent = parent;
    left->right = node;
    node->left = leftRight;
    node->parent = left;
 
    if (leftRight != NULL) {
        leftRight->parent = node;
    }
 
    if (node == t->root) {
        t->root = left;
    } else if (parent->left == node) {
        parent->left = left;
    } else {
        parent->right = left;
    }
 
    left->balance++;
    node->balance = -left->balance;
 
    return left;
}
 
Node Tree_RotateLeftRight (Tree t, Node node) {
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
 
    if (leftRightRight != NULL) {
        leftRightRight->parent = node;
    }
 
    if (leftRightLeft != NULL) {
        leftRightLeft->parent = left;
    }
 
    if (node == t->root) {
        t->root = leftRight;
    } else if (parent->left == node) {
        parent->left = leftRight;
    } else {
        parent->right = leftRight;
    }
 
    if (leftRight->balance == 1) {
        node->balance = 0;
        left->balance = -1;
    } else if (leftRight->balance == 0) {
        node->balance = 0;
        left->balance = 0;
    } else {
        node->balance = 1;
        left->balance = 0;
    }
 
    leftRight->balance = 0;
 
    return leftRight;
}
 
Node Tree_RotateRightLeft (Tree t, Node node) {
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
 
    if (rightLeftLeft != NULL) {
        rightLeftLeft->parent = node;
    }
 
    if (rightLeftRight != NULL) {
        rightLeftRight->parent = right;
    }
 
    if (node == t->root) {
        t->root = rightLeft;
    } else if (parent->right == node) {
        parent->right = rightLeft;
    } else {
        parent->left = rightLeft;
    }
 
    if (rightLeft->balance == -1) {
        node->balance = 0;
        right->balance = 1;
    } else if (rightLeft->balance == 0) {
        node->balance = 0;
        right->balance = 0;
    } else {
        node->balance = -1;
        right->balance = 0;
    }
 
    rightLeft->balance = 0;
 
    return rightLeft;
}
 
void print_trunks (struct trunk *p) {
    if (!p) {
        return;
    }
    print_trunks (p->prev);
    printf ("%s", p->str);
}
 
void print_tree (Tree t, Node n, struct trunk *prev, int is_left) {
    if (n == NULL) {
        return;
    }
 
    struct trunk this_disp = { prev, "     " };
    char *prev_str = this_disp.str;
    print_tree (t, n->right, &this_disp, 1);
 
    if (!prev) {
        this_disp.str = "---";
    } else if (is_left) {
        this_disp.str = ".--";
        prev_str = "    |";
    } else {
        this_disp.str = "`--";
        prev->str = prev_str;
    }
 
    print_trunks (&this_disp);
    (t->print) (n->data);
    printf (" (%+d)\n", n->balance);
 
    if (prev) {
        prev->str = prev_str;
    }
    this_disp.str = "    |";
 
    print_tree (t, n->left, &this_disp, 0);
    if (!prev) {
        puts ("");
    }
}
 
Node Node_New (void *data, Node parent) {
    Node n;
 
    n = malloc (sizeof (*n));
    n->parent = parent;
    n->left = NULL;
    n->right = NULL;
    n->data = data;
    n->balance = 0;
 
    return n;
}
