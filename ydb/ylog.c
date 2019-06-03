#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h> // basename

#include "ylog.h"

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

char *ylog_pname(void)
{
    static char unit[256];
    if (unit[0] == 0)
    {
        FILE *stream;
        char *pname = NULL;
        char line[256];
        pid_t pid = getpid();
        snprintf(line, sizeof(line), "/proc/%d/cmdline", pid);
        stream = fopen(line, "r");
        if (stream)
        {
            pname = fgets(line, sizeof(line), stream);
            pname = basename(pname);
            snprintf(unit, sizeof(unit), "%s(%d)", pname, pid);
            fclose(stream);
        }
        else
        {
            snprintf(unit, sizeof(unit), "%d", pid);
        }
        return unit;
    }
    else
        return unit;
}

unsigned int ylog_severity = YLOG_ERROR;
ylog_func ylog_logger = ylog_general;
ylog_func last_logger = NULL;

int ylog_general(
    int severity, const char *func, int line, const char *format, ...)
{
    int len;
    int n = 0;
    va_list args;
    FILE *fp = NULL;
    if (severity <= YLOG_ERROR)
        fp = stderr;
    else
        fp = stdout;
    if (ferror(fp))
        goto end_log;
    len = fprintf(fp, "++ %s::%s::%s:%d: ", ylog_pname(), ylog_severity_str(severity), func, line);
    n += len;
    va_start(args, format);
    len = vfprintf(fp, format, args);
    va_end(args);
    n += len;
end_log:
    return n;
}

char ylog_file_name[256];
FILE *ylog_fp;
void ylog_file_open(const char *format, ...)
{
    if (ylog_fp)
        fclose(ylog_fp);
    va_list args;
    va_start(args, format);
    vsnprintf(ylog_file_name, sizeof(ylog_file_name), format, args);
    va_end(args);
    if (strstr(ylog_file_name, ".fifo"))
    {
        if (access(ylog_file_name, F_OK) != 0)
        {
            if (mkfifo(ylog_file_name, 0666))
                return;
        }
        // int fd = open(ylog_file_name, O_WRONLY);
        int fd = open(ylog_file_name, O_RDWR);
        ylog_fp = fdopen(fd, "w+");
    }
    else
    {
        ylog_fp = fopen(ylog_file_name, "w");
    }
    if (ylog_fp)
    {
        last_logger = ylog_logger;
        ylog_logger = ylog_file;
    }
    return;
}

void ylog_file_close(void)
{
    if (ylog_fp)
    {
        fclose(ylog_fp);
        ylog_fp = NULL;
    }
    if (ylog_file_name[0] == 0)
    {
        ylog_file_name[0] = 0;
        ylog_logger = last_logger;
    }
}

int ylog_file(int severity, const char *func, int line, const char *format, ...)
{
    int n = 0;
    if (!ylog_fp)
    {
        if (ylog_file_name[0] != 0)
        {
            if (strstr(ylog_file_name, ".fifo"))
            {
                if (access(ylog_file_name, F_OK) != 0)
                {
                    if (mkfifo(ylog_file_name, 0666))
                        return 0;
                }
                // int fd = open(ylog_file_name, O_WRONLY);
                int fd = open(ylog_file_name, O_RDWR);
                ylog_fp = fdopen(fd, "w+");
            }
            else
                ylog_fp = fopen(ylog_file_name, "w");
        }
    }
    if (ylog_fp)
    {
        int len;
        va_list args;
        if (ferror(ylog_fp))
            goto close_fp;
        len = fprintf(ylog_fp, "++ %s::%s::%s:%d: ", ylog_pname(), ylog_severity_str(severity), func, line);
        if (len < 0)
            goto close_fp;
        n += len;
        va_start(args, format);
        len = vfprintf(ylog_fp, format, args);
        n += len;
        va_end(args);
        if (len < 0)
            goto close_fp;
        fflush(ylog_fp);
        return len;
    close_fp:
        fclose(ylog_fp);
        ylog_fp = NULL;
        return 0;
    }
    return n;
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