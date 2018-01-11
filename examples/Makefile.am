bin_PROGRAMS=ymldb-tester
ymldb_tester_SOURCES=tester.c
ymldb_tester_CPPFLAGS = -I $(top_srcdir)/ymldb
ymldb_tester_LDFLAGS = -L$(top_srcdir)/ymldb/.libs \
                  -lymldb -lcprops -lyaml
ymldb_tester_CFLAGS = -g -Wall

