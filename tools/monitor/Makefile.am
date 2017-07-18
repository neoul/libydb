bin_PROGRAMS=ymldb-monitor
ymldb_monitor_SOURCES=ymldb-monitor.c
ymldb_monitor_CPPFLAGS = -I $(top_srcdir)/ymldb
ymldb_monitor_LDFLAGS = -L$(top_srcdir)/ymldb/.libs \
                  -lymldb -lcprops -lyaml
ymldb_monitor_CFLAGS = -g3 -Wall
