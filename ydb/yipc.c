#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

#include "yipc.h"

int yipc_create(char *src_id, char *addr)
{
    ydb_res res = YDB_OK;
    ydb *datablock = NULL;
    if (!addr)
        return -1;
    datablock = ydb_open(src_id);
    if (!datablock)
        return -1;
    res = ydb_connect(datablock, addr, "pub:u:r");
    if (res)
        return -1;
    return ydb_fd(datablock);
}

void yipc_destroy(char *src_id)
{
    ydb_close(ydb_get(src_id, NULL));
}

int yipc_send(char *src_id, char *dest_id, const char *format, ...)
{

}

int yipc_recv(char *src_id, char **dest_id, ydb **datablock);;
