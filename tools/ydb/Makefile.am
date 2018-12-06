bin_PROGRAMS=ydb ymldb ymldb-monitor

ydb_SOURCES = ydb-app.c
ydb_CPPFLAGS = -I $(top_srcdir)/ydb
ydb_LDFLAGS = -L$(top_srcdir)/ydb/.libs \
                  -lydb -lyaml -lm
ydb_CFLAGS = -g -Wall

ymldb_SOURCES = ymldb-app.c
ymldb_CPPFLAGS = -I $(top_srcdir)/ydb
ymldb_LDFLAGS = -L$(top_srcdir)/ydb/.libs \
                  -lydb -lyaml -lm
ymldb_CFLAGS = -g -Wall

ymldb_monitor_SOURCES = ymldb-monitor.c
ymldb_monitor_CPPFLAGS = -I $(top_srcdir)/ydb
ymldb_monitor_LDFLAGS = -L$(top_srcdir)/ydb/.libs \
                  -lydb -lyaml -lm
ymldb_monitor_CFLAGS = -g -Wall
