bin_PROGRAMS=ydb-server ydb-client
ydb_server_SOURCES = server.c
ydb_server_CPPFLAGS = -I $(top_srcdir)/ycomm
ydb_server_LDFLAGS = -L$(top_srcdir)/ycomm/.libs \
                  -lycomm -lyaml -lm
ydb_server_CFLAGS = -g -Wall

ydb_client_SOURCES = client.c
ydb_client_CPPFLAGS = -I $(top_srcdir)/ycomm
ydb_client_LDFLAGS = -L$(top_srcdir)/ycomm/.libs \
                  -lycomm -lyaml -lm
ydb_client_CFLAGS = -g -Wall