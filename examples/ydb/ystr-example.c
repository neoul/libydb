#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ylog.h"
#include "ystr.h"
#include "ylist.h"

int main(int argc, char *argv[])
{
    char buf[512];
    char *key, *line;
    FILE *fp = fopen(argv[1], "r");
    if(!fp)
    {
        printf("[EXIT] no argument\n");
        return -1;
    }
    // ylog_level = YLOG_DEBUG;
    ylist *list = ylist_create();

    while((line = fgets(buf, 512, fp)) != NULL)
    {
        char *newline = strchr(line, '\n');
        if(newline)
            *newline=0;
        key = (char *) ystrdup(line);
        if (ylog_level < YLOG_DEBUG)
            printf("key (%p) %s\n", key, key);
        ylist_push_back(list, key);
    }

    key = ylist_pop_front(list);
    while(key)
    {
        yfree(key);
        key = ylist_pop_front(list);
    }

    ylist_destroy(list);
    // yfree_all();
    fclose(fp);
    return 0;
}