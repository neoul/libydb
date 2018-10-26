bin_PROGRAMS=ydb # ydb-monitor
ydb_SOURCES = ydb-app.c
ydb_CPPFLAGS = -I $(top_srcdir)/ydb
ydb_LDFLAGS = -L$(top_srcdir)/ydb/.libs \
                  -lydb -lyaml -lm
ydb_CFLAGS = -g -Wall

# ydb_monitor_SOURCES = monitor.c
# ydb_monitor_CPPFLAGS = -I $(top_srcdir)/ydb
# ydb_monitor_LDFLAGS = -L$(top_srcdir)/ydb/.libs \
#                   -lydb -lyaml -lm
# ydb_monitor_CFLAGS = -g -Wall
