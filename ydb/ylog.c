#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h> // basename
#include <time.h>

#include "ylog.h"

char *ylog_level_str(int level)
{
    switch (level)
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

char *ylog_process_name(int pid, char *buf, int buflen)
{
    FILE *stream;
    char *pname = NULL;
    char line[256];
    pid_t _pid = (pid_t) pid;
    if (!buf || buflen <= 0)
        return "";
    snprintf(line, sizeof(line), "/proc/%d/cmdline", _pid);
    stream = fopen(line, "r");
    if (stream)
    {
        pname = fgets(line, sizeof(line), stream);
        pname = basename(pname);
        snprintf(buf, buflen, "%s", pname);
        fclose(stream);
    }
    else
    {
        buf[0] = 0;
    }
    return buf;
}

char *ylog_pname(void)
{
    static pid_t pid;
    static char unit[256];
    if (unit[0] == 0 || pid != getpid())
    {
        ylog_process_name(getpid(), unit, sizeof(unit));
        if (unit[0] == 0)
            snprintf(unit, sizeof(unit), "%d", getpid());
    }
    return unit;
}

unsigned int ylog_level = YLOG_ERROR;
ylog_func ylog_logger = ylog_general;
ylog_func last_logger = NULL;

int ylog_general(
    int level, const char *func, int line, const char *format, ...)
{
    int len;
    int n = 0;
    va_list args;
    FILE *fp = NULL;
    if (level <= YLOG_ERROR)
        fp = stderr;
    else
        fp = stdout;
    if (ferror(fp))
        goto end_log;
    len = fprintf(fp, "++ %s.%d::%s::%s:%d: ", ylog_pname(), getpid(), ylog_level_str(level), func, line);
    n += len;
    va_start(args, format);
    len = vfprintf(fp, format, args);
    va_end(args);
    n += len;
    fflush(fp);
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

int ylog_file(int level, const char *func, int line, const char *format, ...)
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
        len = fprintf(ylog_fp, "++ %s.%d::%s::%s:%d: ", ylog_pname(), getpid(), ylog_level_str(level), func, line);
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
    int level, const char *func, int line, const char *format, ...)
{
    return 0;
}

int ylog_register(ylog_func func)
{
    ylog_logger = func;
    return 0;
}

static char *_tz_offset(time_t t)
{
    static char tz[32];
    struct tm local = *localtime(&t);
    struct tm utc = *gmtime(&t);
    long diff = ((local.tm_hour - utc.tm_hour) * 60 + (local.tm_min - utc.tm_min)) * 60L + (local.tm_sec - utc.tm_sec);
    int delta_day = local.tm_mday - utc.tm_mday;
    if ((delta_day == 1) || (delta_day < -1))
    {
        diff += 24L * 60 * 60;
    }
    else if ((delta_day == -1) || (delta_day > 1))
    {
        diff -= 24L * 60 * 60;
    }
    diff = diff / 60; // sec --> min
    int min = diff % 60;
    int hour = diff / 60;
    snprintf(tz, 32, "%s%02d:%02d", (hour >= 0) ? "+" : "-", abs(hour), abs(min));
    return tz;
}

char *ylog_datetime(void)
{
    static char timebuf[64];
    time_t cur_time;
    struct tm *c_tm;
    time(&cur_time);
    c_tm = localtime(&cur_time);
    snprintf(timebuf, 64, "%d-%02d-%02dT%02d:%02d:%02d%s",
                 c_tm->tm_year + 1900, c_tm->tm_mon + 1, c_tm->tm_mday,
                 c_tm->tm_hour, c_tm->tm_min, c_tm->tm_sec,
                 _tz_offset(cur_time));
    return timebuf;
}