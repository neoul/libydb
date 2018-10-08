bin_PROGRAMS=ydb-publisher ydb-subscriber
ydb_publisher_SOURCES = publisher.c
ydb_publisher_CPPFLAGS = -I $(top_srcdir)/ycomm
ydb_publisher_LDFLAGS = -L$(top_srcdir)/ycomm/.libs \
                  -lycomm -lyaml -lm
ydb_publisher_CFLAGS = -g -Wall

ydb_subscriber_SOURCES = subscriber.c
ydb_subscriber_CPPFLAGS = -I $(top_srcdir)/ycomm
ydb_subscriber_LDFLAGS = -L$(top_srcdir)/ycomm/.libs \
                  -lycomm -lyaml -lm
ydb_subscriber_CFLAGS = -g -Wall