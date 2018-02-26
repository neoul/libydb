bin_PROGRAMS=ymldb-monitor
ymldb_monitor_SOURCES=ymldb-monitor.c
ymldb_monitor_CPPFLAGS = -I $(top_srcdir)/ymldb
ymldb_monitor_LDFLAGS = -L$(top_srcdir)/ymldb/.libs -L$(top_srcdir)/ycomm/.libs \
                  -lymldb -lcprops -lyaml -lycomm
ymldb_monitor_CFLAGS = -g -Wall
