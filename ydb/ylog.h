#ifndef __YLOG__
#define __YLOG__

// Logging API for YAML DataBlock

#ifdef __cplusplus
extern "C"
{
#endif

// YAML DataBlock log level
#define YLOG_DEBUG 5
#define YLOG_INOUT 4
#define YLOG_INFO 3
#define YLOG_WARN 2
#define YLOG_ERROR 1
#define YLOG_CRITICAL 0
extern unsigned int ylog_level;
#define ylog_severity ylog_level

// set custom-logging func to get YDB log
typedef int (*ylog_func)(int level, const char *func, int line, const char *format, ...);
int ylog_register(ylog_func func);

// e.g. ylog_file_open("ylog%d.log", 1);
void ylog_file_open(const char *format, ...);
void ylog_file_close(void);
int ylog_file(int level, const char *func, int line, const char *format, ...);
int ylog_general(int level, const char *func, int line, const char *format, ...);
int ylog_quiet(int level, const char *func, int line, const char *format, ...);

extern ylog_func ylog_logger;
#define ylog(level, format, ...)                                       \
    do                                                                    \
    {                                                                     \
        if (ylog_level < (level))                                   \
            break;                                                        \
        ylog_logger(level, __func__, __LINE__, format, ##__VA_ARGS__); \
    } while (0)

#define ylog_debug(format, ...) ylog(YLOG_DEBUG, format, ##__VA_ARGS__)
#define ylog_inout() ylog(YLOG_INOUT, "{{ ... }}\n")
#define ylog_in() ylog(YLOG_INOUT, "{{ ...\n")
#define ylog_out() ylog(YLOG_INOUT, "... }}\n")
#define ylog_info(format, ...) ylog(YLOG_INFO, format, ##__VA_ARGS__)
#define ylog_warn(format, ...) ylog(YLOG_WARN, format, ##__VA_ARGS__)
#define ylog_error(format, ...) ylog(YLOG_ERROR, format, ##__VA_ARGS__)

#define YLOG_SEVERITY_DEBUG (ylog_level >= YLOG_DEBUG)
#define YLOG_SEVERITY_INFO (ylog_level >= YLOG_INFO)

// internal functions
char *ylog_level_str(int level);
char *ylog_process_name(int pid, char *buf, int buflen);
char *ylog_pname(void);
char *ylog_datetime(void);

#ifdef __cplusplus
} // closing brace for extern "C"
#endif

#endif // __YLOG__
