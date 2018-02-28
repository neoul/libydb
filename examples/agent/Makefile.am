bin_PROGRAMS=ymldb-agent
ymldb_agent_SOURCES=agent.c
ymldb_agent_CPPFLAGS = -I $(top_srcdir)/ymldb
ymldb_agent_LDFLAGS = -L$(top_srcdir)/ymldb/.libs -L$(top_srcdir)/ycomm/.libs \
                  -lymldb -lyaml -lycomm
ymldb_agent_CFLAGS = -g -Wall

