#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>
#include "ylog.h"
#include "ytrie.h"
#include "ytree.h"
// #define YALLOC_DEBUG 1

struct ystr
{
    int size;
    unsigned int ref;
    unsigned char str[];
};
static unsigned char empty_ystr[sizeof(int) +
                                sizeof(unsigned int) +
                                sizeof(unsigned long long) +
                                1] = {
    0,
};
static struct ystr *empty = (struct ystr *)empty_ystr;
static ytrie *ystr_pool = NULL;
static ytree *yptr_pool = NULL;

static inline struct ystr *ystr_new(unsigned char *data, int datasize)
{
    struct ystr *str;
    int len = sizeof(struct ystr) + (sizeof(unsigned char) * datasize);
    len = len + ((len%2)?1:2);
    str = malloc(len);
    str->ref = 1;
    str->size = datasize;
    memcpy(str->str, data, datasize);
    str->str[datasize] = 0;
    // printf("data=%s size=%d (%d)\n", str->str, datasize, len);
    return str;
}

const char *ystrndup(char *src, int srclen)
{
    struct ystr *str;
    if (src == NULL)
    {
        empty->ref++;
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] %s str(%p) ref=%d\n", getpid(), __func__, empty->str, empty->ref);
#endif
        return (char *)empty->str;
    }
    if (srclen <= 0)
    {
        empty->ref++;
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] %s str(%p) ref=%d\n", getpid(), __func__, empty->str, empty->ref);
#endif
        return (char *)empty->str;
    }

    if (!ystr_pool)
    {
        ystr_pool = ytrie_create();
        assert(ystr_pool);
#ifdef YALLOC_DEBUG
        ylog_debug("ystr_pool created\n");
#endif
    }

    if (!yptr_pool)
    {
        yptr_pool = ytree_create(NULL, NULL);
        assert(yptr_pool);
#ifdef YALLOC_DEBUG
        ylog_debug("yptr_pool created\n");
#endif
    }

    str = ytrie_search(ystr_pool, src, srclen);
    if (str)
    {
        str->ref++;
    }
    else
    {
        str = ystr_new((unsigned char *) src, srclen);
        if (!str)
            return NULL;
        void *old_ystr;
        old_ystr = ytrie_insert(ystr_pool, (const void *)str->str, srclen, str);
        assert(old_ystr == NULL);
        old_ystr = ytree_insert(yptr_pool, str->str, str);
        assert(old_ystr == NULL);
    }
#ifdef YALLOC_DEBUG
    ylog_debug("[pid %d] %s str(%p,size=%d)=%s ref=%d\n", getpid(), __func__, str, str->size, str->str, str->ref);
#endif
    return (char *)str->str;
}

const char *ystrdup(char *src)
{
    return ystrndup(src, src ? strlen(src) : 0);
}

const void *ydatadup(void *src, int srclen)
{
    struct ystr *str;
    if (src == NULL)
    {
        return NULL;
    }
    if (srclen <= 0)
    {
        return NULL;
    }

    if (!ystr_pool)
    {
        ystr_pool = ytrie_create();
        assert(ystr_pool);
#ifdef YALLOC_DEBUG
        ylog_debug("ystr_pool created\n");
#endif
    }

    if (!yptr_pool)
    {
        yptr_pool = ytree_create(NULL, NULL);
        assert(yptr_pool);
#ifdef YALLOC_DEBUG
        ylog_debug("yptr_pool created\n");
#endif
    }

    str = ytrie_search(ystr_pool, src, srclen);
    if (str)
    {
        str->ref++;
    }
    else
    {
        str = ystr_new((unsigned char *)src, srclen);
        if (!str)
            return NULL;
        void *old_ystr;
        old_ystr = ytrie_insert(ystr_pool, (char *)str->str, srclen, str);
        assert(old_ystr == NULL);
        old_ystr = ytree_insert(yptr_pool, str->str, str);
        assert(old_ystr == NULL);
    }
#ifdef YALLOC_DEBUG
    ylog_debug("[pid %d] %s str(%p,size=%d)=... ref=%d\n", getpid(), __func__, str, str->size, str->ref);
#endif
    return (char *)str->str;
}

const char *ystrnew(const char *format, ...)
{
    struct ystr *str;
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

    if (!ystr_pool)
    {
        ystr_pool = ytrie_create();
        assert(ystr_pool);
#ifdef YALLOC_DEBUG
        ylog_debug("ystr_pool created\n");
#endif
    }

    if (!yptr_pool)
    {
        yptr_pool = ytree_create(NULL, NULL);
        assert(yptr_pool);
#ifdef YALLOC_DEBUG
        ylog_debug("yptr_pool created\n");
#endif
    }

    str = ytrie_search(ystr_pool, src, srclen);
    if (str)
    {
        str->ref++;
        free(src);
    }
    else
    {
        str = ystr_new((unsigned char *)src, srclen);
        if (!str)
            return NULL;
        void *old_ystr;
        old_ystr = ytrie_insert(ystr_pool, (const void *)str->str, srclen, str);
        assert(old_ystr == NULL);
        old_ystr = ytree_insert(yptr_pool, str->str, str);
        assert(old_ystr == NULL);
    }
#ifdef YALLOC_DEBUG
    ylog_debug("[pid %d] %s str(%p,size=%d)=%s ref=%d\n", getpid(), __func__, str, str->size, str->str, str->ref);
#endif
    return (char *)str->str;
empty:
    empty->ref++;
#ifdef YALLOC_DEBUG
    ylog_debug("[pid %d] %s str(%p) ref=%d\n", getpid(), __func__, empty->str, empty->ref);
#endif
    return (const char *)empty->str;
}

struct ystr *ysearch(const void *src, int srclen)
{
    struct ystr *str;
    if (src == NULL)
        return NULL;
    if (yptr_pool)
    {
        str = ytree_search(yptr_pool, (void *) src);
        if (str)
            return str;
    }
    if (ystr_pool && srclen > 0)
    {
        str = ytrie_search(ystr_pool, src, srclen);
        if (str)
            return str;
    }
    return NULL;
}

void yfree(const void *src)
{
    struct ystr *str;
    if (!yptr_pool)
    {
        assert(!ystr_pool);
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] no yptr_pool\n", getpid());
#endif
        return;
    }

    if (!src)
    {
        empty->ref--;
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] %s str(%p) ref=%d\n", getpid(), __func__, empty->str, empty->ref);
#endif
        return;
    }
    if (src == empty->str)
    {
        empty->ref--;
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] %s str(%p) ref=%d\n", getpid(), __func__, empty->str, empty->ref);
#endif
        return;
    }

    str = ysearch(src, 0);
    if (str)
    {
        str->ref--;
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] %s str(%p,size=%d)=%s ref=%d\n", getpid(), __func__, str, str->size, str->str, str->ref);
#endif
        if (str->ref <= 0)
        {
            ytrie_delete(ystr_pool, str->str, str->size);
            ytree_delete(yptr_pool, str->str);
            free(str);
        }
    }
    else
    {
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] %s no ystr (%p)\n", getpid(), __func__, src);
#endif
    }
    if (ytrie_size(ystr_pool) <= 0)
    {
        ytrie_destroy(ystr_pool);
        ystr_pool = NULL;
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] %s ystr_pool destroyed\n", getpid(), __func__);
#endif
        assert(yptr_pool);
    }
    if (ytree_size(yptr_pool) <= 0)
    {
        ytree_destroy(yptr_pool);
        yptr_pool = NULL;
#ifdef YALLOC_DEBUG
        ylog_debug("[pid %d] %s yptr_pool destroyed\n", getpid(), __func__);
#endif
        assert(!ystr_pool);
    }
    return;
}

static void ystr_free(void *v)
{
    struct ystr *str = v;
    free(str);
    return;
}

void yfree_all()
{
    if (yptr_pool)
        ytree_destroy_custom(yptr_pool, ystr_free);
    if (ystr_pool)
        ytrie_destroy(ystr_pool);
}
