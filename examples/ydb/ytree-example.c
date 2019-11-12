#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include "ytree.h"

#define MAX_KEY_VALUE 100

struct ukey
{
    int key;
    double value;
};

struct udata
{
    char str[32];
};

int comp(void *a1, void *a2)
{
    struct ukey *ud1 = (struct ukey *)a1;
    struct ukey *ud2 = (struct ukey *)a2;

    if (ud1->key < ud2->key)
        return -1;
    else if (ud1->key > ud2->key)
        return +1;
    else
        return 0;
}

int _user_cb_ukey(void *key, void *data, void *user_data)
{
    int *count = (int *)user_data;
    struct ukey *ukey = (struct ukey *)key;

    (*count)++;
    printf("TRAVERSE: %d. - %d(^%.3f)\n", *count, ukey->key, ukey->value);
    return 0;
}

int _user_cb_udata(void *key, void *data, void *user_data)
{
    int *count = (int *)user_data;
    struct ukey *ukey = (struct ukey *)key;
    struct udata *udata = (struct udata *)data;

    (*count)++;
    printf("TRAVERSE: %d. - %d %s\n", *count, ukey->key, udata->str);
    return 0;
}

void _user_free(void *v)
{
    struct udata *udata = v;
    printf("%s is removed\n", udata->str);
    free(v);
}

void _key_free(void *v)
{
    struct ukey *ukey = v;
    printf("!REMOVED: %d\n", ukey->key);
    free(v);
}

int main(int argc, char **argv)
{
    ytree *tree;
    struct ukey *ukey = NULL;
    struct udata *udata = NULL;
    int count = 0;

    printf("\nYTREE case 1)\n");
    printf("\n[key == data]\n");
    tree = ytree_create(comp, NULL);

    for (count = 1; count <= 10; count++)
    {
        ukey = malloc(sizeof(*ukey));
        ukey->key = rand() % MAX_KEY_VALUE;
        ukey->value = sqrt(ukey->key);

        // the same (duplicated) key value will be removed by _key_free.
        ytree_insert_custom(tree, ukey, ukey, _key_free);
        printf("INSERT: %d. (%d)\n", count, ukey->key);
    }
    count = 0;
    ytree_traverse(tree, _user_cb_ukey, &count);

    for (count = 0; count < MAX_KEY_VALUE; count++)
    {
        struct ukey searchkey;
        searchkey.key = count;
        udata = ytree_delete(tree, &searchkey);
        if (udata)
            _key_free(udata);
    }
    ytree_destroy(tree);

    printf("\nYTREE case 2)\n");
    printf("\n[key != data]\n");
    tree = ytree_create(comp, _key_free);
    for (count = 1; count <= 10; count++)
    {
        ukey = malloc(sizeof(*ukey));
        ukey->key = rand() % MAX_KEY_VALUE;
        ukey->value = sqrt(ukey->key);

        udata = malloc(sizeof(*udata));
        sprintf(udata->str, "@@%d", ukey->key);
        printf("INSERT: %d. (%d: %s)\n", count, ukey->key, udata->str);

        // old key (ukey) will be removed by _key_free configured on ytree_create()
        // but, you still need to remove the data.
        struct udata *old = ytree_insert(tree, ukey, udata);
        if (old)
        {
            printf("old data is returned. (%s)\n", udata->str);
            _user_free(old);
        }
    }

    printf("\n[deletion in iteration]\n");
    count = 0;
    ytree_iter *iter = ytree_first(tree);
    for (; iter != NULL; iter = ytree_next(tree, iter))
    {
        struct udata *old = NULL;
        count++;
        ukey = ytree_key(iter);
        udata = ytree_data(iter);
        printf("ITERATE: %d. - %d %s\n", count, ukey->key, udata->str);
        if (count == 1)
            iter = ytree_remove(tree, iter, (void *) &old);
        else if (count == 5)
            iter = ytree_remove(tree, iter, (void *) &old);
        else if (ytree_size(tree) < count) // remove data at tail.
            iter = ytree_remove(tree, iter, (void *) &old);
        if (old)
            _user_free(old);

    }
    

    printf("\n[back in iteration]\n");
    count = 0;
    iter = ytree_last(tree);
    for (; iter != NULL; iter = ytree_prev(tree, iter))
    {
        count++;
        ukey = ytree_key(iter);
        udata = ytree_data(iter);
        printf("ITERATE BACK: %d. - %d %s\n", count, ukey->key, udata->str);
    }

    count = 0;
    struct ukey low;
    struct ukey high;
    low.key = 20;
    high.key = 50;
    printf("\nTRAVERSE IN RANGE from %d to %d\n", low.key, high.key);
    ytree_traverse_in_range(tree, &low, &high, _user_cb_udata, &count);

    struct ukey nkey;
    nkey.key = 28;
    iter = ytree_find_nearby(tree, &nkey, 0);
    printf("\nSEARCH an node that is the nearest %d\n", nkey.key);
    printf(" %d=%s\n",
           ((struct ukey *)(ytree_key(iter)))->key,
           ((struct udata *)ytree_data(iter))->str);

    // ytree_destroy_custom() must be called for freeing all data.
    ytree_destroy_custom(tree, _user_free);
    return 0;
}