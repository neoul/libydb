#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#ifdef YALLOC_DEBUG
#include "ylog.h"
#endif
#include "ytrie.h"
#include "ytree.h"

#ifdef MEM_POOL
static ytree mem_pool = NULL;
#else
static unsigned int mem_count = 0;
#endif
static ytrie *string_pool = NULL;
struct ystr_alloc
{
    char *key; // ymldb key
    int ref;   // reference count;
};
static char emptystr[4] = {0, 0, 0, 0};
static struct ystr_alloc empty = { .key = emptystr, .ref = 0};

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
        {
            mem_count++;
        }
    }
#endif
    return p;
}


const char *ystrndup(char *src, int srclen)
{
    struct ystr_alloc *ykey;
    if(src == NULL)
    {
        empty.ref++;
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] ystrdup src=NULL, return empty.key %p\n", getpid(), empty.key);
#endif
        return empty.key;
    }
    if (srclen <= 0)
    {
        empty.ref++;
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] ystrdup srclen<= 0, return empty.key %p\n", getpid(), empty.key);
#endif
        return empty.key;
    }

    if (!string_pool)
    {
        string_pool = ytrie_create();
        if (!string_pool)
            return NULL;
#ifdef YALLOC_DEBUG
        ylog_debug("string pool created\n");
#endif
    }
    
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
        ykey->key = strndup(src, srclen);
        assert(ykey->key);
        ykey->ref = 1;
        void *old_ykey = ytrie_insert(string_pool, ykey->key, strlen(ykey->key), ykey);
        assert(old_ykey == NULL);
    }
#ifdef YALLOC_DEBUG
    ylog_debug("[pid %d] ystrdup ykey=%p key=%s (%p) ref=%d \n", getpid(), ykey, ykey->key, ykey->key, ykey->ref);
#endif
    return ykey->key;
}

const char *ystrdup(char *src)
{
    return ystrndup(src, src?strlen(src):0);
}

void yfree(const void *src)
{
    if(!src)
    {
        empty.ref--;
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] yfree src=NULL, return empty.key %p\n", getpid(), empty.key);
#endif
        return;
    }
    if (src == empty.key)
    {
        empty.ref--;
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] yfree src=empty.key, return empty.key %p\n", getpid(), empty.key);
#endif
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
#ifdef YALLOC_DEBUG
            ylog_debug("[pid %d] yfree ykey=%p key=%s (%p) ref=%d \n", getpid(), ykey, ykey->key, ykey->key, ykey->ref);
#endif
            if (ykey->ref <= 0)
            {
                ytrie_delete(string_pool, ykey->key, srclen);
                free(ykey->key);
                free(ykey);
            }
        }
        else
        {
#ifdef YALLOC_DEBUG
            ylog_debug("[pid %d] yfree search failed - key=%s (%p)\n", getpid(), src, src);
#endif
        }
        
        if (ytrie_size(string_pool) <= 0)
        {
#ifdef YALLOC_DEBUG
            ylog_debug("[pid %d] string pool destroyed\n", getpid());
#endif
            ytrie_destroy(string_pool);
            string_pool = NULL;
        }
        return;
    }
    else
    {
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] yfree failed - no string pool\n", getpid());
#endif
    }
#ifndef MEM_POOL
    if(src)
    {
        mem_count--;
        free((void *) src);
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