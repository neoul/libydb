#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yarray.h"

int callback(int index, void *data, void *addition)
{
    int *value = (int *) data;
    printf("index=%d data=%d (%p)\n", index, *value, data);
    return 0;
}

int main()
{
    int count;
    yarray *a = yarray_create(16);
    int *data;

    for (count=0; count<10; count++)
    {
        data = malloc(sizeof(int));
        *data = count;
        yarray_push_back(a, data);
    }

    yarray_traverse(a, callback, NULL);
    yarray_fprintf(stdout, a);

    for (count=10; count<20; count++)
    {
        data = malloc(sizeof(int));
        *data = count;
        yarray_push_front(a, data);
    }

    yarray_traverse(a, callback, NULL);
    yarray_fprintf(stdout, a);

    for (count=0; count<20; count++)
    {
        data = malloc(sizeof(int));
        *data = count + 1000;
        yarray_insert(a, count, data);
    }

    yarray_traverse(a, callback, NULL);
    yarray_fprintf(stdout, a);

    count = yarray_search_around(a, 10, data);
    printf("\n %p count=%d\n\n", data, count);
    count = yarray_search_around(a, 100, data);
    printf("\n %p count=%d\n\n", data, count);

    for (count=0; count<20; count++)
        yarray_delete_custom(a, count, free);
    printf("===============\n");

    for (count = 0; count < yarray_size(a); count++)
    {
        data = yarray_data(a, count);
        printf("index loop: data[%d]=%d\n", count, *data);
    }

    for (count = 0; count < 10; count++)
    {
        data = yarray_pop_front(a);
        if (data)
        {
            printf("data[%d]=%d\n", count, *data);
            free(data);
        }
    }
    yarray_destroy_custom(a, free);
    return 0;
}

