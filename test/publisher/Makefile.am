bin_PROGRAMS=publisher
publisher_SOURCES=ymldb-publisher.c
publisher_CPPFLAGS = -I $(top_srcdir)/ymldb
publisher_LDFLAGS = -L$(top_srcdir)/ymldb/.libs \
                  -lymldb -lcprops -lyaml
