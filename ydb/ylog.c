#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>

#include "ylog.h"

int ylog_general(
    int severity, const char *func, int line, const char *format, ...)
{
    int len = -1;
    va_list args;
    FILE *fp = NULL;
    switch (severity)
    {
    case YLOG_DEBUG:
        fp = stdout;
        if (fp)
            fprintf(fp, "** ydb::debug::%s:%d: ", func, line);
        break;
    case YLOG_INOUT:
        fp = stdout;
        if (fp)
            fprintf(fp, "** ydb::inout:%s:%d: ", func, line);
        break;
    case YLOG_INFO:
        fp = stdout;
        if (fp)
            fprintf(fp, "** ydb::info::%s:%d: ", func, line);
        break;
    case YLOG_WARN:
        fp = stdout;
        if (fp)
            fprintf(fp, "** ydb::warn::%s:%d: ", func, line);
        break;
    case YLOG_ERROR:
        fp = stderr;
        if (fp)
            fprintf(fp, "** ydb::error:%s:%d: ", func, line);
        break;
    case YLOG_CRITICAL:
        fp = stderr;
        if (fp)
            fprintf(fp, "** ydb::critical:%s:%d: ", func, line);
        break;
    default:
        return 0;
    }
    va_start(args, format);
    len = vfprintf(fp, format, args);
    va_end(args);
    return len;
}

int ylog_quiet(
    int severity, const char *func, int line, const char *format, ...)
{
    return 0;
}

unsigned int ylog_severity = YLOG_ERROR;
ylog_func ylog_logger = ylog_general;
int ylog_register(ylog_func func)
{
    ylog_logger = func;
    return 0;
}