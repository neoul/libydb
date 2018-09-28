#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

#include "yarray.h"

struct _yfragment
{
    int n;
    int front;
    int fsize;
    ylist_iter *iter;
    void *data[];
};

typedef struct _yfragment yfragment;

struct _yarray
{
    ylist *fragments;
    size_t fsize;
    size_t n;
};

#define YINDEX(fra, index) (((fra)->front + (index)) % (fra)->fsize)

static yfragment *yfragment_new(size_t size)
{
    yfragment *fra;
    size_t s = sizeof(yfragment) + (size * sizeof(void *));
    fra = malloc(s);
    if (fra)
    {
        memset(fra, 0x0, s);
    }
    fra->fsize = size;
    return fra;
}

void yfragment_free(yfragment *fra, user_free ufree)
{
    int i;
    if (!fra)
        return;
    if (ufree)
    {
        for (i = 0; i < fra->n; i++)
        {
            int pos = YINDEX(fra, i);
            if (fra->data[pos])
                ufree(fra->data[pos]);
        }
    }
    free(fra);
}

static yfragment *yfragment_get(yarray *array, int index, int *local_index)
{
    yfragment *fra;
    if (index < 0)
        return NULL;
    if (index >= array->n)
        return NULL;
    if (index < array->n / 2)
    {
        int n = 0;
        ylist_iter *iter = ylist_first(array->fragments);
        for (; !ylist_done(iter); iter = ylist_next(iter))
        {
            fra = ylist_data(iter);
            if (index < n + fra->n)
            {
                *local_index = index - n;
                return fra;
            }
            n = n + fra->n;
        }
    }
    else
    {
        int n = array->n;
        ylist_iter *iter = ylist_last(array->fragments);
        for (; !ylist_done(iter); iter = ylist_prev(iter))
        {
            fra = ylist_data(iter);
            n = n - fra->n;
            if (index >= n)
            {
                *local_index = index - n;
                return fra;
            }
        }
    }
    return NULL;
}

yfragment *yfragment_shift_next(yarray *array, yfragment *fra, int *local_index)
{
    int lindex = *local_index;
    yfragment *tar_fra = ylist_data(ylist_next(fra->iter));
    if (!tar_fra || (tar_fra && ((tar_fra->fsize - tar_fra->n) < (fra->n - lindex))))
    {
        tar_fra = yfragment_new(fra->fsize);
        if (!tar_fra)
            return NULL;
        tar_fra->iter = ylist_insert(array->fragments, fra->iter, tar_fra);
        if (!tar_fra->iter)
        {
            free(tar_fra);
            return NULL;
        }
    }
    int n;
    int max = fra->n - lindex;
    tar_fra->front = (tar_fra->fsize + tar_fra->front - max) % tar_fra->fsize;
    for (n = 0; n < max; n++)
    {
        int cur_index = YINDEX(fra, lindex + n);
        tar_fra->data[YINDEX(tar_fra, n)] = fra->data[cur_index];
        fra->data[cur_index] = NULL;
    }
    fra->n = fra->n - max;
    tar_fra->n = tar_fra->n + max;
    return fra;
}

yfragment *yfragment_shift_prev(yarray *array, yfragment *fra, int *local_index)
{
    int lindex = *local_index;
    yfragment *tar_fra = NULL;
    tar_fra = ylist_data(ylist_prev(fra->iter));

    if (!tar_fra || (tar_fra && ((tar_fra->fsize - tar_fra->n) < (lindex + 1))))
    {
        tar_fra = yfragment_new(fra->fsize);
        if (!tar_fra)
            return NULL;
        if (ylist_prev(fra->iter))
            tar_fra->iter = ylist_insert(array->fragments, ylist_prev(fra->iter), tar_fra);
        else
            tar_fra->iter = ylist_push_front(array->fragments, tar_fra);
        if (!tar_fra->iter)
        {
            free(tar_fra);
            return NULL;
        }
    }
    int n;
    int max = lindex;
    for (n = 0; n < max; n++)
    {
        int cur_index = YINDEX(fra, n);
        tar_fra->data[YINDEX(tar_fra, tar_fra->n + n)] = fra->data[cur_index];
        fra->data[cur_index] = NULL;
    }
    fra->front = (fra->fsize + fra->front + max) % fra->fsize;
    fra->n = fra->n - max;
    tar_fra->n = tar_fra->n + max;
    *local_index = tar_fra->n;
    return tar_fra;
}

int yfragment_merge_next(yarray *array, yfragment *fra)
{
    yfragment *tar_fra = ylist_data(ylist_next(fra->iter));
    if (tar_fra)
    {
        int n;
        // merge it to fra
        if (((fra->fsize - fra->n) >= tar_fra->n) && fra->n >= tar_fra->n)
        {
            int max = tar_fra->n;
            for (n = 0; n < max; n++)
                fra->data[YINDEX(fra, fra->n + n)] = tar_fra->data[YINDEX(tar_fra, n)];
            fra->n = fra->n + max;
            ylist_erase(tar_fra->iter, free);
            return 1;
        }
        else if (((tar_fra->fsize - tar_fra->n) >= fra->n) && tar_fra->n >= fra->n)
        {
            int max = fra->n;
            tar_fra->front = (tar_fra->fsize + tar_fra->front - max) % tar_fra->fsize;
            for (n = 0; n < max; n++)
            {
                int cur_index = YINDEX(fra, n);
                tar_fra->data[YINDEX(tar_fra, n)] = fra->data[cur_index];
            }
            tar_fra->n = tar_fra->n + max;
            ylist_erase(fra->iter, free);
            return 1;
        }
    }
    return 0;
}

int yfragment_merge_prev(yarray *array, yfragment *fra)
{
    yfragment *tar_fra = ylist_data(ylist_prev(fra->iter));
    if (tar_fra)
        return yfragment_merge_next(array, tar_fra);
    return 0;
}

// create a array
yarray *yarray_create(int fsize)
{
    yarray *array;
    if (fsize < 4)
        fsize = 4;
    array = malloc(sizeof(yarray));
    if (array)
    {
        array->fragments = ylist_create();
        if (!array->fragments)
        {
            free(array);
            return NULL;
        }
        array->fsize = fsize;
        array->n = 0;
    }
    return array;
}

// destroy the array and the data is also free by ufree()
void yarray_destroy_custom(yarray *array, user_free ufree)
{
    if (array)
    {
        yfragment *fra;
        fra = ylist_pop_front(array->fragments);
        while (fra)
        {
            yfragment_free(fra, ufree);
            fra = ylist_pop_front(array->fragments);
        }
        ylist_destroy(array->fragments);
        free(array);
    }
}

// destroy the array
void yarray_destroy(yarray *array)
{
    yarray_destroy_custom(array, NULL);
}

// return index if success otherwise return -1
int yarray_push_back(yarray *array, void *data)
{
    int index = -1;
    yfragment *fra;
    if (!array || !data)
        return index;
    fra = ylist_back(array->fragments);
    if (fra)
    {
        if (fra->n < fra->fsize)
        {
            index = array->n;
            fra->data[YINDEX(fra, fra->n)] = data;
            fra->n++;
            array->n++;
            return index;
        }
    }

    fra = yfragment_new(array->fsize);
    if (!fra)
        return index;
    fra->iter = ylist_push_back(array->fragments, fra);
    if (!fra->iter)
    {
        free(fra);
        return index;
    }
    index = array->n;
    fra->data[0] = data;
    fra->n++;
    array->n++;
    return index;
}

// return index if success otherwise return -1
int yarray_push_front(yarray *array, void *data)
{
    yfragment *fra;
    if (!array || !data)
        return -1;
    fra = ylist_front(array->fragments);
    if (fra)
    {
        if (fra->n < fra->fsize)
        {
            fra->front = (fra->fsize + fra->front - 1) % fra->fsize;
            fra->data[YINDEX(fra, 0)] = data;
            fra->n++;
            array->n++;
            return 0;
        }
    }

    fra = yfragment_new(array->fsize);
    if (!fra)
        return -1;
    fra->iter = ylist_push_front(array->fragments, fra);
    if (!fra->iter)
    {
        free(fra);
        return -1;
    }
    fra->data[0] = data;
    fra->n++;
    array->n++;
    return 0;
}

// return last data from yarray
void *yarray_pop_back(yarray *array)
{
    void *data = NULL;
    yfragment *fra;
    if (!array)
        return NULL;
    fra = ylist_back(array->fragments);
    if (fra)
    {
        int index = YINDEX(fra, fra->n - 1);
        data = fra->data[index];
        fra->data[index] = NULL;
        fra->n--;
        array->n--;
        if (fra->n <= 0)
        {
            ylist_pop_back(array->fragments);
            free(fra);
        }
    }
    return data;
}

// return last data from yarray
void *yarray_pop_front(yarray *array)
{
    void *data = NULL;
    yfragment *fra;
    if (!array)
        return NULL;
    fra = ylist_front(array->fragments);
    if (fra)
    {
        int index = YINDEX(fra, 0);
        data = fra->data[index];
        fra->data[index] = NULL;
        fra->front = (fra->front + 1) % fra->fsize;
        fra->n--;
        array->n--;
        if (fra->n <= 0)
        {
            ylist_pop_front(array->fragments);
            free(fra);
        }
    }
    return data;
}

int yarray_empty(yarray *array)
{
    if (array && array->n)
        return 0;
    return 1;
}

int yarray_size(yarray *array)
{
    if (array && array->n)
        return array->n;
    return 0;
}

void *yarray_data(yarray *array, int index)
{
    int local_index = 0;
    yfragment *fra = yfragment_get(array, index, &local_index);
    if (fra)
        return fra->data[YINDEX(fra, local_index)];
    return NULL;
}

void *yarray_delete(yarray *array, int index)
{
    int n;
    int pos;
    int merged;
    int local_index = 0;
    yfragment *fra;
    void *data = NULL;
    if (!array || index < 0)
        return NULL;
    if (index > array->n)
        return NULL;
    fra = yfragment_get(array, index, &local_index);
    if (!fra)
        return NULL;
    pos = YINDEX(fra, local_index);
    data = fra->data[pos];
    if (local_index <= fra->n / 2)
    {
        for (n = local_index - 1; n >= 0; n--)
            fra->data[YINDEX(fra, n + 1)] = fra->data[YINDEX(fra, n)];
        fra->data[YINDEX(fra, 0)] = NULL;
        fra->front = (fra->front + 1) % fra->fsize;
    }
    else
    {
        int max = fra->n - local_index - 1;
        for (n = 0; n < max; n++)
            fra->data[YINDEX(fra, local_index + n)] = fra->data[YINDEX(fra, local_index + n + 1)];
        fra->data[YINDEX(fra, fra->n - 1)] = NULL;
    }
    fra->n--;
    array->n--;

    merged = yfragment_merge_next(array, fra);
    if (!merged)
        yfragment_merge_prev(array, fra);
    yarray_fprintf(stdout, array);
    return data;
}

void yarray_delete_custom(yarray *array, int index, user_free ufree)
{
    void *data = yarray_delete(array, index);
    if (data && ufree)
        ufree(data);
}

// return index if success otherwise return -1 or -2
int yarray_insert(yarray *array, int index, void *data)
{
    int n;
    int local_index = 0;
    yfragment *fra;
    if (!array || !data || index < 0)
        return -1;
    if (index > array->n)
        return -1;
    fra = yfragment_get(array, index, &local_index);
    if (!fra)
        return -1;
    if (fra->n >= fra->fsize)
    {
        if (local_index > fra->n / 2)
            fra = yfragment_shift_next(array, fra, &local_index);
        else
            fra = yfragment_shift_prev(array, fra, &local_index);
        if (!fra)
            return -2;
    }

    if (local_index <= fra->n / 2)
    {
        fra->front = (fra->fsize + fra->front - 1) % fra->fsize;
        for (n = 0; n < local_index; n++)
            fra->data[YINDEX(fra, n)] = fra->data[YINDEX(fra, n + 1)];
    }
    else
    {
        int max = fra->n - local_index;
        for (n = 0; n < max; n++)
            fra->data[YINDEX(fra, fra->n - n)] = fra->data[YINDEX(fra, fra->n - 1 - n)];
    }
    fra->data[YINDEX(fra, local_index)] = data;
    fra->n++;
    array->n++;
    yarray_fprintf(stdout, array);
    return index;
}

void yarray_fprintf(FILE *fp, yarray *array)
{
    ylist_iter *iter;
    yfragment *fra;
    if (!array)
        return;
    fprintf(fp, "array: {p: %p, fsize: %ld, n: %ld, fragments: %d}\n",
            array, array->fsize, array->n, ylist_size(array->fragments));
    for (iter = ylist_first(array->fragments);
         !ylist_done(iter); iter = ylist_next(iter))
    {
        int i;
        fra = ylist_data(iter);
        fprintf(fp, " - array->flagments: {p: %p, fsize: %d, front %d, n: %d, ",
                fra, fra->fsize, fra->front, fra->n);
        fprintf(fp, " data: [");
        for (i = 0; i < fra->fsize; i++)
            fprintf(fp, " %s,", fra->data[i] ? "O" : " ");
        fprintf(fp, "] }\n");
    }
}

int yarray_traverse(yarray *array, yarray_callback cb, void *addition)
{
    int res = -1;
    int index;
    ylist_iter *iter;
    yfragment *fra;
    if (!array)
        return res;
    index = 0;
    for (iter = ylist_first(array->fragments);
         !ylist_done(iter); iter = ylist_next(iter))
    {
        int local_index;
        fra = ylist_data(iter);
        for (local_index = 0; local_index < fra->n; local_index++)
        {
            int pos = YINDEX(fra, local_index);
            res = cb(index, fra->data[pos], addition);
            if (res)
                return res;
            index += 1;
        }
    }
    return res;
}

static int yarray_search_local(yfragment *fra, void *data)
{
    int local_index = 0;

    for (; local_index < fra->n; local_index++)
    {
        int pos = YINDEX(fra, local_index);
        if (fra->data[pos] == data)
            return local_index;
    }
    return -1;
}

int yarray_search_around(yarray *array, int around, void *data)
{
    int index = -1;
    int local_index = 0;
    yfragment *fra;
    if (!array || !data)
        return -1;
    fra = yfragment_get(array, around, &local_index);
    // printf("fra=%p local_index=%d\n", fra, local_index);
    if (!fra)
    {
        ylist_iter *iter;
        index = 0;
        for (iter = ylist_first(array->fragments);
             !ylist_done(iter); iter = ylist_next(iter))
        {
            fra = ylist_data(iter);
            local_index = yarray_search_local(fra, data);
            // printf("fra=%p local_index=%d\n", fra, local_index);
            if (local_index < 0)
                index += fra->n;
            else
                return index + local_index;
        }
    }
    else
    {
        index = 0;
        ylist_iter *_next = fra->iter;
        ylist_iter *_prev = ylist_prev(fra->iter);
        while (_next || _prev)
        {
            if (_next)
            {
                fra = ylist_data(_next);
                local_index = yarray_search_local(fra, data);
                // printf("_next fra=%p local_index=%d\n", fra, local_index);
                if (local_index >= 0)
                {
                    ylist_iter *iter;
                    for (iter = ylist_first(array->fragments);
                         !ylist_done(iter); iter = ylist_next(iter))
                    {
                        fra = ylist_data(iter);
                        if (_next == iter)
                            break;
                        index += fra->n;
                    }
                    return index + local_index;
                }
                _next = ylist_next(_next);
            }

            if (_prev)
            {
                fra = ylist_data(_prev);
                local_index = yarray_search_local(fra, data);
                // printf("_prev fra=%p local_index=%d\n", fra, local_index);
                if (local_index > 0)
                {
                    ylist_iter *iter;
                    for (iter = ylist_first(array->fragments);
                         !ylist_done(iter); iter = ylist_next(iter))
                    {
                        fra = ylist_data(iter);
                        if (_prev == iter)
                            break;
                        index += fra->n;
                    }
                    return index + local_index;
                }
                _prev = ylist_prev(_prev);
            }
        }
    }
    return -1;
}