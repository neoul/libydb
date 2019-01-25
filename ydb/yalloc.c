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

static unsigned int mem_count = 0;
static ytrie *yalloc_pool = NULL;
struct ydata
{
    unsigned char *key;
    int ref;   // reference count;
};
static unsigned char emptystr[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static struct ydata empty = { .key = emptystr, .ref = 0};

const char *ystrndup(char *src, int srclen)
{
    struct ydata *ydata;
    if(src == NULL)
    {
        empty.ref++;
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] ystrdup src=NULL, return empty.key %p\n", getpid(), empty.key);
#endif
        return (char *) empty.key;
    }
    if (srclen <= 0)
    {
        empty.ref++;
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] ystrdup srclen<= 0, return empty.key %p\n", getpid(), empty.key);
#endif
        return (char *) empty.key;
    }

    if (!yalloc_pool)
    {
        yalloc_pool = ytrie_create();
        if (!yalloc_pool)
            return NULL;
#ifdef YALLOC_DEBUG
        ylog_debug("string pool created\n");
#endif
    }
    
    ydata = ytrie_search(yalloc_pool, src, srclen);
    if (ydata)
    {
        ydata->ref++;
    }
    else
    {
        ydata = malloc(sizeof(struct ydata));
        if (!ydata)
            return NULL;
        ydata->key = (unsigned char *) strndup(src, srclen);
        assert(ydata->key);
        ydata->ref = 1;
        void *old_ydata = ytrie_insert(yalloc_pool, (char *) ydata->key, strlen((char *) ydata->key), ydata);
        assert(old_ydata == NULL);
    }
#ifdef YALLOC_DEBUG
    ylog_debug("[pid %d] ystrdup ydata=%p key=%s (%p) ref=%d \n", getpid(), ydata, ydata->key, ydata->key, ydata->ref);
#endif
    return (char *) ydata->key;
}

const char *ystrdup(char *src)
{
    return ystrndup(src, src?strlen(src):0);
}

const void *ydatadup(void *src, int srclen)
{
    struct ydata *ydata;
    if(src == NULL)
    {
        empty.ref++;
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] ydatadup src=NULL, return empty.key %p\n", getpid(), empty.key);
#endif
        return empty.key;
    }
    if (srclen <= 0)
    {
        empty.ref++;
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] ydatadup srclen<= 0, return empty.key %p\n", getpid(), empty.key);
#endif
        return empty.key;
    }
    
    if (!yalloc_pool)
    {
        yalloc_pool = ytrie_create();
        if (!yalloc_pool)
            return NULL;
#ifdef YALLOC_DEBUG
        ylog_debug("string pool created\n");
#endif
    }
    
    ydata = ytrie_search(yalloc_pool, src, srclen);
    if (ydata)
    {
        ydata->ref++;
    }
    else
    {
        ydata = malloc(sizeof(struct ydata));
        if (!ydata)
            return NULL;
        ydata->key = malloc(srclen);
        assert(ydata->key);
        memcpy(ydata->key, src, srclen);
        ydata->ref = 1;
        void *old_ydata = ytrie_insert(yalloc_pool, (char *) ydata->key, strlen((char *) ydata->key), ydata);
        assert(old_ydata == NULL);
    }
#ifdef YALLOC_DEBUG
    ylog_debug("[pid %d] ydatadup ydata=%p key=%s (%p) ref=%d \n", getpid(), ydata, ydata->key, ydata->key, ydata->ref);
#endif
    return ydata->key;
}

const void *ysearch(void *src, int srclen)
{
    if(src == NULL || srclen <= 0)
        return NULL;
    if (yalloc_pool)
    {
        struct ydata *ydata;
        ydata = ytrie_search(yalloc_pool, src, srclen);
        if (ydata)
            return ydata->key;
    }
    return NULL;
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

    if (yalloc_pool)
    {
        struct ydata *ydata;
        int srclen = strlen(src);
        ydata = ytrie_search(yalloc_pool, src, srclen);
        if (ydata)
        {
            ydata->ref--;
#ifdef YALLOC_DEBUG
            ylog_debug("[pid %d] yfree ydata=%p key=%s (%p) ref=%d \n", getpid(), ydata, ydata->key, ydata->key, ydata->ref);
#endif
            if (ydata->ref <= 0)
            {
                ytrie_delete(yalloc_pool, ydata->key, srclen);
                free(ydata->key);
                free(ydata);
            }
        }
        else
        {
#ifdef YALLOC_DEBUG
            ylog_debug("[pid %d] yfree search failed - key=%s (%p)\n", getpid(), src, src);
#endif
        }
        
        if (ytrie_size(yalloc_pool) <= 0)
        {
#ifdef YALLOC_DEBUG
            ylog_debug("[pid %d] string pool destroyed\n", getpid());
#endif
            ytrie_destroy(yalloc_pool);
            yalloc_pool = NULL;
        }
        return;
    }
    else
    {
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] yfree failed - no string pool\n", getpid());
#endif
    }

    if(src)
    {
        mem_count--;
        free((void *) src);
    }
}

int string_traverse(void *data, const void *key, int key_len, void *value)
{
    struct ydata *ydata = value;
    printf((char *)data, ydata->key, ydata->ref);
    return 0;
}


void ydata_print()
{
    if (yalloc_pool)
        ytrie_traverse(yalloc_pool, string_traverse, "ydata:%s:%d\n");
    printf("mem_count %u\n", mem_count);
}

static void ydata_free(void *v)
{
    struct ydata *ydata = v;
    if (ydata->key)
        free(ydata->key);
    free(ydata);
    return;
}

void yfree_all()
{
    if (yalloc_pool)
        ytrie_destroy_custom(yalloc_pool, ydata_free);
}