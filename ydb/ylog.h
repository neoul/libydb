#ifndef __YLOG__
#define __YLOG__

// Logging API for YAML DataBlock

#ifdef __cplusplus
extern "C"
{
#endif

// YAML DataBlock log severity
#define YLOG_DEBUG 5
#define YLOG_INOUT 4
#define YLOG_INFO 3
#define YLOG_WARN 2
#define YLOG_ERROR 1
#define YLOG_CRITICAL 0
extern unsigned int ylog_severity;

// set custom-logging func to get YDB log
typedef int (*ylog_func)(int severity, const char *func, int line, const char *format, ...);
int ylog_register(ylog_func func);

// e.g. ylog_file_open("ylog%d.log", 1);
void ylog_file_open(const char *format, ...);
void ylog_file_close();
int ylog_file(int severity, const char *func, int line, const char *format, ...);
int ylog_general(int severity, const char *func, int line, const char *format, ...);
int ylog_quiet(int severity, const char *func, int line, const char *format, ...);

extern ylog_func ylog_logger;
#define ylog(severity, format, ...)                                       \
    do                                                                    \
    {                                                                     \
        if (ylog_severity < (severity))                                   \
            break;                                                        \
        ylog_logger(severity, __func__, __LINE__, format, ##__VA_ARGS__); \
    } while (0)

#define ylog_debug(format, ...) ylog(YLOG_DEBUG, format, ##__VA_ARGS__)
#define ylog_inout() ylog(YLOG_INOUT, "\n")
#define ylog_in() ylog(YLOG_INOUT, "{{ ------\n")
#define ylog_out() ylog(YLOG_INOUT, "}}\n")
#define ylog_info(format, ...) ylog(YLOG_INFO, format, ##__VA_ARGS__)
#define ylog_warn(format, ...) ylog(YLOG_WARN, format, ##__VA_ARGS__)
#define ylog_error(format, ...) ylog(YLOG_ERROR, format, ##__VA_ARGS__)

#define YLOG_SEVERITY_DEBUG (ylog_severity >= YLOG_DEBUG)
#define YLOG_SEVERITY_INFO (ylog_severity >= YLOG_INFO)

// internal functions
char *ylog_severity_str(int severity);
char *ylog_pname();

#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#endif // __YLOG__
