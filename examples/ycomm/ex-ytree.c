#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "ytree.h"
 
#define MAX_KEY_VALUE 100
 
typedef struct udata {
    int    key;
    double value;
} *udata;
 
int comp (void *a1, void *a2) {
    udata ud1 = (udata) a1;
    udata ud2 = (udata) a2;
 
    if (ud1->key < ud2->key) {
        return -1;
    } else if (ud1->key > ud2->key) {
        return +1;
    } else {
        return 0;
    }
}
 
void print (void *a) {
    udata ud = (udata) a;
 
    printf ("%3d \"%6.3f\"", ud->key, ud->value);
}

int done = 0;

void signal_handler_INT(int param)
{
    done = 1;
}

int user_cb (void *data, void *user_data)
{
    udata ud = (udata) data;
    printf (" - %s %3d(^%6.3f)\n", (char *)user_data, ud->key, ud->value);
    return 0;
}
 
int main (int argc, char **argv) {
    ytree tree;
    udata ud1 = NULL, ud2;
    int count = 0;
 
    tree = ytree_create (comp, print);
 
    while (!done) {
        count++;
        if(count > 20) break;
        if (ud1 == NULL) {
            ud1 = malloc (sizeof (*ud1));
        }
        ud1->key = rand () % MAX_KEY_VALUE;
        ud1->value = sqrt(ud1->key);

        if ((ud2 = ytree_search (tree, ud1)) != NULL) {
            printf (">>> delete key %d\n\n", ud2->key);
            ytree_delete(tree, ud2);
            free (ud2);
            free (ud1);
        } else {
            printf (">>> insert key %d\n\n", ud1->key);
            ytree_insert (tree, ud1);
        }
        ytree_printf (tree);
        ud1 = NULL;
    }
    if(ud1)
        free(ud1);
    printf("%d = ytree_size()\n", ytree_size(tree));

    ytree_traverse(tree, user_cb, "~~~");

    struct udata udLow;
    struct udata udHigh;
    udLow.key = 16;
    udHigh.key = 90;
    ytree_traverse_in_range(tree, &udLow, &udHigh, user_cb, "===");

    ytree_printf(tree);

    ytree_destroy_custom(tree, free);

    return 0;
}