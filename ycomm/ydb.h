#ifndef __YDB__
#define __YDB__

#include <stdio.h>

#define YDB_LEVEL_MAX 32

typedef enum _ydb_res
{
    YDB_OK = 0,
    YDB_ERR,
	YDB_E_NO_ARGS,
	YDB_E_TYPE_ERR,
    YDB_E_INVALID_PARENT,
    YDB_E_NO_ENTRY,
    YDB_E_DUMP_CB,
    YDB_E_MEM,
    YDB_E_FULL_BUF,
    YDB_E_PERSISTENCY_ERR,
    YDB_E_INVALID_YAML_INPUT,
    YDB_E_INVALID_YAML_TOP,
    YDB_E_INVALID_YAML_KEY,
    YDB_E_INVALID_YAML_ENTRY,
    YDB_E_YAML_INIT,
    YDB_E_YAML_EMPTY_TOKEN,
    YDB_E_MERGE_FAILED,
} ydb_res;

#define YDB_VNAME(NAME) #NAME
extern char *ydb_err_str[];

#define YDB_LOG_DBG     3
#define YDB_LOG_INFO    2
#define YDB_LOG_ERR     1
#define YDB_LOG_CRI     0

// set the ydb log severity
extern unsigned int ydb_log_severity;
typedef int (*ydb_log_func)(int severity, const char *func, int line, const char *format, ...);
int ydb_log_register( ydb_log_func func);

extern ydb_log_func ydb_logger;
#define ydb_log(severity, format, ...)                                               \
    do                                                                               \
    {                                                                                \
        if (ydb_log_severity < (severity))                                           \
            break;                                                                   \
        if (ydb_logger)                                                              \
            ydb_logger(severity, (__FUNCTION__), (__LINE__), format, ##__VA_ARGS__); \
    } while (0)

#define ydb_log_debug(format, ...) ydb_log(YDB_LOG_DBG, format, ##__VA_ARGS__)
#define ydb_log_info(format, ...) ydb_log(YDB_LOG_INFO, format, ##__VA_ARGS__)
#define ydb_log_error(format, ...) ydb_log(YDB_LOG_ERR, format, ##__VA_ARGS__)
#define ydb_log_res(res) ydb_log(YDB_LOG_ERR, "%s\n", ydb_err_str[res])

#define YDB_LOGGING_DEBUG (ydb_log_severity >= YDB_LOG_DBG)
#define YDB_LOGGING_INFO (ydb_log_severity >= YDB_LOG_INFO)

#include "ynode.h"
// yaml data block
typedef struct _ydb ydb;

// open local ydb (yaml data block)
ydb *ydb_open(char *path);
// close local ydb
void ydb_close(ydb *datablock);

// get the ydb top
ynode *ydb_top();

// update ydb using the input string
ydb_res ydb_write(ydb *datablock, const char *format, ...);

#endif // __YDB__
