#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ylist.h"

struct user_data
{
    int id;
    char str[32];
};

void user_print (void *data, void *add)
{
    struct user_data *user = data;
    printf(" - user_data[%d]\n", user->id);
}

int main()
{
    int i;
    ylist_iter *iter;
    ylist *list = ylist_create();
    struct user_data *data;

    iter = ylist_first(list);
    data = malloc(sizeof(struct user_data));
    data->id = 1000;
    sprintf(data->str, "user[%d]", data->id);
    ylist_insert(iter, data);

    for (i = 0; i < 10; i++)
    {
        data = malloc(sizeof(struct user_data));
        if (data)
        {
            data->id = i; // rand() % 64;
            sprintf(data->str, "user[%d]", data->id);
            ylist_push_back(list, data);
            printf("ADD user[%d]\n", data->id);
        }
    }
    ylist_printf(list, user_print, NULL);

    for (i = 0; i < 3; i++)
    {
        data = ylist_pop_front(list);
        if (data)
        {
            printf("DEL %s\n", data->str);
            free(data);
        }
    }
    for (i = 0; i < 3; i++)
    {
        data = ylist_pop_back(list);
        if (data)
        {
            printf("DEL %s\n", data->str);
            free(data);
        }
    }
    for (i = 20; i < 22; i++)
    {
        data = malloc(sizeof(struct user_data));
        if (data)
        {
            data->id = i; // rand() % 64;
            sprintf(data->str, "user[%d]", data->id);
            ylist_push_back(list, data);
            printf("ADD user[%d]\n", data->id);
        }
    }

    for (iter = ylist_first(list);
         !ylist_done(iter);
         iter = ylist_next(iter))
    {
        data = ylist_data(iter);
        printf("LIST user[%d]\n", data->id);
    }

    // iter = ylist_prev(iter);
    iter = ylist_next(iter);

    data = malloc(sizeof(struct user_data));
    data->id = 100;
    sprintf(data->str, "user[%d]", data->id);
    ylist_insert(iter, data);
    printf("INSERT user[%d]\n", data->id);

    for (iter = ylist_last(list);
         !ylist_done(iter);
         iter = ylist_prev(iter))
    {
        data = ylist_data(iter);
        printf("LIST user[%d]\n", data->id);
    }

    for (iter = ylist_first(list);
         !ylist_done(iter);
         iter = ylist_next(iter))
    {
        data = ylist_data(iter);
        if (data->id > 10)
            iter = ylist_erase(iter, free);
    }
    printf("After ERASE!!\n");
    for (iter = ylist_last(list);
         !ylist_done(iter);
         iter = ylist_prev(iter))
    {
        data = ylist_data(iter);
        printf("LIST user[%d]\n", data->id);
    }

    ylist_destroy_custom(list, free);
    return 0;
}
