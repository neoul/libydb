#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include "ylog.h"
#include "ytrie.h"
#include "ytree.h"

static unsigned int mem_count = 0;
static ytrie *ystr_pool = NULL;
struct ydata
{
    unsigned char *key;
    int ref; // reference count;
};
static unsigned char emptystr[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static struct ydata empty = {.key = emptystr, .ref = 0};

const char *ystrndup(char *src, int srclen)
{
    struct ydata *ydata;
    if (src == NULL)
    {
        empty.ref++;
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] ystrdup src=NULL, return empty.key %p\n", getpid(), empty.key);
#endif
        return (char *)empty.key;
    }
    if (srclen <= 0)
    {
        empty.ref++;
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] ystrdup srclen<= 0, return empty.key %p\n", getpid(), empty.key);
#endif
        return (char *)empty.key;
    }

    if (!ystr_pool)
    {
        ystr_pool = ytrie_create();
        if (!ystr_pool)
            return NULL;
#ifdef YALLOC_DEBUG
        ylog_debug("ystr_pool created\n");
#endif
    }

    ydata = ytrie_search(ystr_pool, src, srclen);
    if (ydata)
    {
        ydata->ref++;
    }
    else
    {
        ydata = malloc(sizeof(struct ydata));
        if (!ydata)
            return NULL;
        ydata->key = (unsigned char *)strndup(src, srclen);
        assert(ydata->key);
        ydata->ref = 1;
        void *old_ydata = ytrie_insert(ystr_pool, (char *)ydata->key, strlen((char *)ydata->key), ydata);
        assert(old_ydata == NULL);
    }
#ifdef YALLOC_DEBUG
    ylog_debug("[pid %d] ystrdup ydata=%p key=%s (%p) ref=%d \n", getpid(), ydata, ydata->key, ydata->key, ydata->ref);
#endif
    return (char *)ydata->key;
}

const char *ystrdup(char *src)
{
    return ystrndup(src, src ? strlen(src) : 0);
}

const void *ydatadup(void *src, int srclen)
{
    struct ydata *ydata;
    if (src == NULL)
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

    if (!ystr_pool)
    {
        ystr_pool = ytrie_create();
        if (!ystr_pool)
            return NULL;
#ifdef YALLOC_DEBUG
        ylog_debug("ystr_pool created\n");
#endif
    }

    ydata = ytrie_search(ystr_pool, src, srclen);
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
        void *old_ydata = ytrie_insert(ystr_pool, (char *)ydata->key, srclen, ydata);
        assert(old_ydata == NULL);
    }
#ifdef YALLOC_DEBUG
    ylog_debug("[pid %d] ydatadup ydata=%p key=%s (%p) ref=%d \n", getpid(), ydata, ydata->key, ydata->key, ydata->ref);
#endif
    return ydata->key;
}

const char *ystrnew(const char *format, ...)
{
    char *src = NULL;
    size_t srclen = 0;
    FILE *fp;
    if (!format)
        goto empty;
    fp = open_memstream(&src, &srclen);
    if (!fp)
        goto empty;
    {
        va_list args;
        va_start(args, format);
        vfprintf(fp, format, args);
        va_end(args);
    }
    fclose(fp);
    if (!src || srclen <= 0)
    {
        if (src)
            free(src);
        src = NULL;
        goto empty;
    }

    struct ydata *ydata;
    if (!ystr_pool)
    {
        ystr_pool = ytrie_create();
        if (!ystr_pool)
            return NULL;
#ifdef YALLOC_DEBUG
        ylog_debug("ystr_pool created\n");
#endif
    }

    ydata = ytrie_search(ystr_pool, src, srclen);
    if (ydata)
    {
        ydata->ref++;
        free(src);
    }
    else
    {
        ydata = malloc(sizeof(struct ydata));
        if (!ydata)
            return NULL;
        ydata->key = (unsigned char *)src;
        assert(ydata->key);
        ydata->ref = 1;
        void *old_ydata = ytrie_insert(ystr_pool, (char *)ydata->key, srclen, ydata);
        assert(old_ydata == NULL);
    }
#ifdef YALLOC_DEBUG
    ylog_debug("[pid %d] ystrnew ydata=%p key=%s (%p) ref=%d \n", getpid(), ydata, ydata->key, ydata->key, ydata->ref);
#endif
    return (char *)ydata->key;
empty:
    empty.ref++;
#ifdef YALLOC_DEBUG
    ylog_debug("[pid %d] ystrnew srclen<= 0, return empty.key %p\n", getpid(), empty.key);
#endif
    return (const char *)empty.key;
}

const void *ysearch(void *src, int srclen)
{
    if (src == NULL || srclen <= 0)
        return NULL;
    if (ystr_pool)
    {
        struct ydata *ydata;
        ydata = ytrie_search(ystr_pool, src, srclen);
        if (ydata)
            return ydata->key;
    }
    return NULL;
}

void yfree(const void *src)
{
    if (!src)
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

    if (ystr_pool)
    {
        struct ydata *ydata;
        int srclen = strlen(src);
        ydata = ytrie_search(ystr_pool, src, srclen);
        if (ydata)
        {
            ydata->ref--;
#ifdef YALLOC_DEBUG
            ylog_debug("[pid %d] yfree ydata=%p key=%s (%p) ref=%d \n", getpid(), ydata, ydata->key, ydata->key, ydata->ref);
#endif
            if (ydata->ref <= 0)
            {
                ytrie_delete(ystr_pool, ydata->key, srclen);
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

        if (ytrie_size(ystr_pool) <= 0)
        {
#ifdef YALLOC_DEBUG
            ylog_debug("[pid %d] ystr_pool destroyed\n", getpid());
#endif
            ytrie_destroy(ystr_pool);
            ystr_pool = NULL;
        }
        return;
    }
    else
    {
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] yfree failed - no ystr_pool\n", getpid());
#endif
    }

    if (src)
    {
        mem_count--;
        free((void *)src);
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
    if (ystr_pool)
        ytrie_traverse(ystr_pool, string_traverse, "ydata:%s:%d\n");
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
    if (ystr_pool)
        ytrie_destroy_custom(ystr_pool, ydata_free);
}