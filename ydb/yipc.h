#ifndef __YIPC__
#define __YIPC__

// yipc is simple inter-process communication API.
//  - implemented by YAML DataBlock (YDB)
//  - easy data handling by using YDB API
//  - easy communication among processes by using a few functions

#include "ydb.h"

#ifdef __cplusplus
extern "C"
{
#endif

int yipc_create(char *src_id, char *addr);

void yipc_destroy(char *src_id);

int yipc_send(char *src_id, char *dest_id, const char *format, ...);

int yipc_send_sync(char *src_id, char *dest_id, const char *format, ...);

int yipc_recv(char *src_id, int timeout, ydb **datablock);

int yipc_state(char *id);

#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#endif // __YIPC__