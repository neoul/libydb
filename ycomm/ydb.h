#ifndef __YDB__
#define __YDB__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#define YDB_LEVEL_MAX 32
#define YDB_CONN_MAX 32

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
    YDB_E_SYSTEM_FAILED,
    YDB_E_CONN_FAILED,
    YDB_E_CONN_CLOSED,
    YDB_E_CONN_DENIED,
    YDB_E_INVALID_MSG,
} ydb_res;

#define YDB_VNAME(NAME) #NAME
extern char *ydb_err_str[];

#define YDB_LOG_DBG     5
#define YDB_LOG_INOUT   4
#define YDB_LOG_INFO    3
#define YDB_LOG_WARN    2
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
#define ydb_log_inout() ydb_log(YDB_LOG_INOUT, "\n")
#define ydb_log_in() ydb_log(YDB_LOG_INOUT, "{{------\n")
#define ydb_log_out() ydb_log(YDB_LOG_INOUT, " ------}}\n")
#define ydb_log_info(format, ...) ydb_log(YDB_LOG_INFO, format, ##__VA_ARGS__)
#define ydb_log_warn(format, ...) ydb_log(YDB_LOG_WARN, format, ##__VA_ARGS__)
#define ydb_log_error(format, ...) ydb_log(YDB_LOG_ERR, format, ##__VA_ARGS__)
#define ydb_log_res(res) ydb_log(YDB_LOG_ERR, "%s\n", ydb_err_str[res])

#define YDB_LOGGING_DEBUG (ydb_log_severity >= YDB_LOG_DBG)
#define YDB_LOGGING_INFO (ydb_log_severity >= YDB_LOG_INFO)

#include "ynode.h"
// yaml data block
typedef struct _ydb ydb;

// open ydb (yaml data block)
ydb *ydb_open(char *path, char *addr, char *flags);
// close local ydb
void ydb_close(ydb *datablock);

// get the ydb
ydb *ydb_get(char *path);

// return the top ynode of ydb or the global root ynode of all ydb.
ynode *ydb_top(ydb *datablock);

// update delete ydb using the input string (yaml format)
ydb_res ydb_write(ydb *datablock, const char *format, ...);
ydb_res ydb_delete(ydb *datablock, const char *format, ...);

// read the date from ydb as the scanf() (yaml format)
int ydb_read(ydb *datablock, const char *format, ...);

// update & delete the ydb using input path and value
// ydb_path_write(datablock, "/path/to/update=%d", value)
ydb_res ydb_path_write(ydb *datablock, const char *format, ...);
ydb_res ydb_path_delete(ydb *datablock, const char *format, ...);

// read the value from ydb using input path
// char *value = ydb_path_read(datablock, "/path/to/update")
char *ydb_path_read(ydb *datablock, const char *format, ...);

int ydb_serve(ydb *datablock, int timeout);

#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#endif // __YDB__
