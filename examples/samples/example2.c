#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <time.h>

#include "ymldb.h"

int ymldb_input(int num)
{
    // write a file for a ymldb stream input.
    FILE *f;
    if (num == 0) 
        f = fopen("ymldb-example2.yml", "w");
    else
        f = fopen("ymldb-example2.yml", "a");
    
    if (!f)
    {
        fprintf(stderr, "%s", strerror(errno));
        return -1;
    }

    fprintf(f,
        "%%TAG !merge! ymldb:op:merge\n"
        "---\n"
        "key%d:\n"
        "  identifier: %d\n"
        "...\n"
        ,
        num, num
        );
    fclose(f);
    return 0;
}

int main(int argc, char *argv[])
{
    int res;
    int count;
    for (count = 0; count < 100; count++)
    {
        res = ymldb_input(count);
        if (res)
        {
            break;
        }
    }

    clock_t start, end;
    double cpu_time_used;
    start = clock();

    // ymldb_log_set(YMLDB_LOG_LOG, NULL);
    
    res = ymldb_create("example", YMLDB_FLAG_NONE);
    if (res < 0)
    {
        fprintf(stderr, "ymldb create failed.\n");
        return -1;
    }

    // read ymldb from a file.
    printf("\n\n[ymldb_run_with_fd]\n");
    int infd = open("ymldb-example2.yml", O_RDONLY, 0644);
    ymldb_run_with_fd("example", infd, STDOUT_FILENO);
    close(infd);

    // printf("\n\n[ymldb_dump]\n");
    // ymldb_dump(stdout, NULL);
 
    ymldb_destroy_all();

    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    fprintf(stdout, "time elapsed: %f\n", cpu_time_used);
    return 0;
}
