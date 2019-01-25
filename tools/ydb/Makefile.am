bin_PROGRAMS=ydb

ydb_SOURCES = ydb-app.c
ydb_CPPFLAGS = -I $(top_srcdir)/ydb
ydb_LDFLAGS = -L$(top_srcdir)/ydb/.libs \
                  -lydb -lyaml -lm
ydb_CFLAGS = -g -Wall