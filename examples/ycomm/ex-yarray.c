#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yarray.h"

int main()
{
    int count;
    yarray *a = yarray_create(8);

    for (count=0; count<10; count++)
    {
        int *data = malloc(sizeof(int));
        *data = count;
        yarray_push_back(a, data);
    }

    for (count=10; count<20; count++)
    {
        int *data = malloc(sizeof(int));
        *data = count;
        yarray_push_front(a, data);
    }

    yarray_fprintf(stdout, a);

    for (count=0; count<20; count++)
    {
        int *data = malloc(sizeof(int));
        *data = count + 1000;
        yarray_insert(a, count, data);
    }
    yarray_fprintf(stdout, a);

    for (count=0; count<20; count++)
        yarray_delete_custom(a, count, free);

    for (count = 0; count < 40; count++)
    {
        int *data = yarray_pop_front(a);
        if (data)
        {
            printf("data[%d]=%d\n", count, *data);
            free(data);
        }
    }

    yarray_destroy_custom(a, free);
    return 0;
}

