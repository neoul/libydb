#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ytrie.h"
#include "ytree.h"

#ifdef MEM_POOL
static ytree mem_pool = NULL;
#else
static unsigned int mem_count = 0;
#endif
static ytrie *string_pool = NULL;
static char empty[4] = {0, 0, 0, 0};

struct ystr_alloc
{
    char *key; // ymldb key
    int ref;   // reference count;
};

void *yalloc(size_t s)
{
#ifdef MEM_POOL
    if (!mem_pool)
    {
        mem_pool = ytree_create(NULL, NULL);
        if (!mem_pool)
            return NULL;
    }

    void *p = malloc(s);
    if (p)
    {
        void *old = ytree_insert(mem_pool, p);
        if (old)
        {
            printf("Invalid mem (%p) alloc/free happens.\n", old);
            return NULL;
        }
        printf("yalloc p=%p\n", p);
    }
#else
    void *p = NULL;
    if(s > 0)
    {
        p = malloc(s);
        if(p)
            mem_count++;
    }
#endif
    return p;
}

const char *ystrdup(char *src)
{
    int srclen;
    struct ystr_alloc *ykey;
    if (!string_pool)
    {
        string_pool = ytrie_create();
        if (!string_pool)
            return NULL;
    }
    if (!src || src[0] == 0)
    {
        return empty;
    }
    srclen = strlen(src);
    ykey = ytrie_search(string_pool, src, srclen);
    if (ykey)
    {
        ykey->ref++;
    }
    else
    {
        ykey = malloc(sizeof(struct ystr_alloc));
        if (!ykey)
            return NULL;
        ykey->key = strdup(src);
        if (!ykey->key)
        {
            free(ykey);
            return NULL;
        }
        ykey->ref = 1;
        void *res = ytrie_insert(string_pool, ykey->key, srclen, ykey);
        if (res != NULL)
        {
            free(ykey->key);
            free(ykey);
            return NULL;
        }
    }
    // printf("ystrdup p=%p p->key=%p key=%s ref=%d \n", ykey, ykey->key, ykey->key, ykey->ref);
    return ykey->key;
}

void yfree(void *src)
{
    if (!src || src == empty)
    {
        return;
    }
#ifdef MEM_POOL
    if (mem_pool)
    {
        void *ok = ytree_delete(mem_pool, src);
        if (ok)
        {
            printf("yfree p=%p\n", src);
            free(ok);
            if (ytree_size(mem_pool) <= 0)
            {
                ytree_destroy(mem_pool);
                mem_pool = NULL;
            }
            return;
        }
    }
#endif
    if (string_pool)
    {
        struct ystr_alloc *ykey;
        int srclen = strlen(src);
        ykey = ytrie_search(string_pool, src, srclen);
        if (ykey)
        {
            ykey->ref--;
            // printf("yfree p=%p p->key=%p key=%s ref=%d \n", ykey, ykey->key, ykey->key, ykey->ref);
            if (ykey->ref <= 0)
            {
                ytrie_delete(string_pool, ykey->key, srclen);
                free(ykey->key);
                free(ykey);
            }
            if (ytrie_size(string_pool) <= 0)
            {
                ytrie_destroy(string_pool);
                string_pool = NULL;
            }
            return;
        }
    }
#ifndef MEM_POOL
    if(src)
    {
        mem_count--;
        // printf("free p=%p\n", src);
        free(src);
    }
#endif
}

int string_traverse(void *data, const void *key, int key_len, void *value)
{
    struct ystr_alloc *ystr = value;
    printf((char *)data, ystr->key, ystr->ref);
    return 0;
}

#ifdef MEM_POOL
int mem_traverse(void *data, void *user_data)
{
    printf((char *)user_data, data);
    return 0;
}
#endif

void ystrprint()
{
    if (string_pool)
        ytrie_traverse(string_pool, string_traverse, "ystr:%s:%d\n");
#ifdef MEM_POOL
    if (mem_pool)
        ytree_traverse(mem_pool, mem_traverse, "yalloc:%p\n");
#else
    printf("mem_count %u\n", mem_count);
#endif
}

void ystring_delete(void *v)
{
    struct ystr_alloc *ystr = v;
    if (ystr->key)
        free(ystr->key);
    free(ystr);
    return;
}

void yalloc_destroy()
{
    if (string_pool)
        ytrie_destroy_custom(string_pool, ystring_delete);
#ifdef MEM_POOL
    if (mem_pool)
        ytree_destroy_custom(mem_pool, free);
#endif
}