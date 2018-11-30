#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#include "yipc.h"
#include "ynode.h"

#define CLEAR_BUF(buf, buflen) \
    do                         \
    {                          \
        if (buf)               \
            free(buf);         \
        buf = NULL;            \
        buflen = 0;            \
    } while (0)

int yipc_create(char *src_id, char *addr)
{
    ydb_res res = YDB_OK;
    ydb *datablock = NULL;
    if (!addr || !src_id)
        return -1;
    datablock = ydb_open(src_id);
    if (!datablock)
        return -1;
    ydb_write(datablock,
              "+meta:\n"
              " {%s: %s}\n",
              src_id, src_id);
    res = ydb_connect(datablock, addr, "pub:r");
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
    ydb_res res = YDB_OK;
    ydb *datablock = NULL;
    ydb_iter *idb = NULL;
    FILE *fp;
    char *buf = NULL;
    size_t buflen = 0;
    char path[256];

    if (!src_id || !dest_id)
        return -1;
    datablock = ydb_get(src_id, NULL);
    if (!datablock)
        return -1;

    fp = open_memstream(&buf, &buflen);
    if (!fp)
        return -3;

    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);
    fclose(fp);

    if (!buf)
        return 0;
    if (buflen <= 0)
    {
        free(buf);
        return 0;
    }
    snprintf(path, sizeof(path), "/+meta/%s", dest_id);
    res = ydb_whisper(datablock,
                    path,
                  "+msg:\n"
                  " src: %s\n"
                  " dest: %s\n"
                  "%s\n",
                  src_id, dest_id, buf);
    CLEAR_BUF(buf, buflen);

    idb = ydb_down(ydb_top(datablock));
    while (idb)
    {
        char *key = ydb_key(idb);
        if (!key)
            break;
        if (strcmp(key, "+meta") != 0)
        {
            ydb_iter *idb_next;
            idb_next = ydb_next(idb);
            ynode_remove(idb);
            idb = idb_next;
        }
        else
            idb = ydb_next(idb);
    }
    if (res)
        return -4;
    return 0;
}

extern ydb_res ydb_recv(ydb *datablock, int timeout, bool once_recv);
int yipc_recv(char *src_id, int timeout, ydb **datablock)
{
    ydb_res res = YDB_OK;
    ydb *db;
    ydb_iter *idb;
    if (!src_id || !datablock)
        return -1;
    db = ydb_get(src_id, NULL);
    if (!db)
        return -1;

    *datablock = NULL;
    idb = ydb_down(ydb_top(db));
    while (idb)
    {
        char *key = ydb_key(idb);
        if (!key)
            break;
        if (strcmp(key, "+meta") != 0)
        {
            ydb_iter *idb_next;
            idb_next = ydb_next(idb);
            ynode_remove(idb);
            idb = idb_next;
        }
        else
            idb = ydb_next(idb);
    }

    res = ydb_recv(db, timeout, true);
    if (res && res != YDB_W_MORE_RECV)
        return -2;

    printf("RECV =====\n");
    ydb_dump(db, stdout);
    printf("=====\n\n");

    idb = ydb_search(db, "+msg/dest");
    if (idb)
    {
        if (strcmp(ydb_value(idb), src_id) == 0)
        {
            *datablock = db;
        }
    }
    if (res == YDB_W_MORE_RECV)
        return 1;
    return 0;
}

int yipc_state(char *id);
