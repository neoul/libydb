bin_PROGRAMS=ymldb-subscriber
ymldb_subscriber_SOURCES=ymldb-subscriber.c
ymldb_subscriber_CPPFLAGS = -I $(top_srcdir)/ymldb
ymldb_subscriber_LDFLAGS = -L$(top_srcdir)/ymldb/.libs \
                  -lymldb -lcprops -lyaml
ymldb_subscriber_CFLAGS = -g3 -Wall
