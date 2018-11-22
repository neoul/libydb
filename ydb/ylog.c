#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>

#include "ylog.h"

unsigned int ylog_severity = YLOG_ERROR;
ylog_func ylog_logger = ylog_general;
ylog_func last_logger = NULL;

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
            fprintf(fp, "___ydb::debug::%s:%d: ", func, line);
        break;
    case YLOG_INOUT:
        fp = stdout;
        if (fp)
            fprintf(fp, "___ydb::inout:%s:%d: ", func, line);
        break;
    case YLOG_INFO:
        fp = stdout;
        if (fp)
            fprintf(fp, "___ydb::info::%s:%d: ", func, line);
        break;
    case YLOG_WARN:
        fp = stdout;
        if (fp)
            fprintf(fp, "___ydb::warn::%s:%d: ", func, line);
        break;
    case YLOG_ERROR:
        fp = stderr;
        if (fp)
            fprintf(fp, "___ydb::error:%s:%d: ", func, line);
        break;
    case YLOG_CRITICAL:
        fp = stderr;
        if (fp)
            fprintf(fp, "___ydb::critical:%s:%d: ", func, line);
        break;
    default:
        return 0;
    }
    va_start(args, format);
    len = vfprintf(fp, format, args);
    va_end(args);
    return len;
}

char *ylog_severity_str(int severity)
{
    switch (severity)
    {
    case YLOG_DEBUG:
        return "debug";
    case YLOG_INOUT:
        return "inout";
    case YLOG_INFO:
        return "info";
    case YLOG_WARN:
        return "warn";
    case YLOG_ERROR:
        return "error";
    case YLOG_CRITICAL:
        return "critical";
    default:
        return "unknown";
    }
}

static char unit[256];
char *ylog_pname()
{
    if (unit[0] == 0)
    {
        FILE *stream;
        char *pname = NULL;
        char cmdline[256];
        pid_t pid = getpid();
        
        snprintf(cmdline, sizeof(cmdline), "/proc/%d/cmdline", pid);
        stream = fopen(cmdline, "r");
        if (stream)
        {
            pname = fgets(unit, sizeof(unit), stream);
            fclose(stream);
        }
        return pname;
    }
    else
        return unit;
}

char ylog_file_name[256];
static FILE *ylog_fp;
void ylog_file_open(const char *format, ...)
{
    if (ylog_fp)
        fclose(ylog_fp);
    va_list args;
    va_start(args, format);
    vsnprintf(ylog_file_name, sizeof(ylog_file_name), format, args);
    va_end(args);
    ylog_fp = fopen(ylog_file_name, "w");
    if (ylog_fp)
    {
        last_logger = ylog_logger;
        ylog_logger = ylog_file;
    }
    return;
}

void ylog_file_close()
{
    if (ylog_fp)
        fclose(ylog_fp);
    if (ylog_file_name[0] == 0)
    {
        ylog_file_name[0] = 0;
        ylog_logger = last_logger;
    }
}


int ylog_file(int severity, const char *func, int line, const char *format, ...)
{
    if (!ylog_fp)
    {
        if (ylog_file_name[0] != 0)
        {
            ylog_fp = fopen(ylog_file_name, "w");
        }
    }
    if (ylog_fp)
    {
        int len = -1;
        va_list args;
        fprintf(ylog_fp, "_ydb::%s::%s::%s:%d: ", ylog_pname(), ylog_severity_str(severity), func, line);
        va_start(args, format);
        len = vfprintf(ylog_fp, format, args);
        va_end(args);
        if (len < 0)
        {
            fclose(ylog_fp);
            ylog_fp = NULL;
        }
        else
            fflush(ylog_fp);
        return len;
    }
    return 0;
}

int ylog_quiet(
    int severity, const char *func, int line, const char *format, ...)
{
    return 0;
}

int ylog_register(ylog_func func)
{
    ylog_logger = func;
    return 0;
}