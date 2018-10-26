bin_PROGRAMS=ymldb-monitor ymldb-agent

ymldb_monitor_SOURCES=monitor.c
ymldb_monitor_CPPFLAGS = -I $(top_srcdir)/ymldb
ymldb_monitor_LDFLAGS = -L$(top_srcdir)/ymldb/.libs -L$(top_srcdir)/ydb/.libs \
                  -lymldb -lyaml -lydb
ymldb_monitor_CFLAGS = -g -Wall

ymldb_agent_SOURCES=agent.c
ymldb_agent_CPPFLAGS = -I $(top_srcdir)/ymldb
ymldb_agent_LDFLAGS = -L$(top_srcdir)/ymldb/.libs -L$(top_srcdir)/ydb/.libs \
                  -lymldb -lyaml -lydb
ymldb_agent_CFLAGS = -g -Wall