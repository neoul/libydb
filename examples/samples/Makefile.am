bin_PROGRAMS=ymldb-example ymldb-update-callback-example ymldb-file-rw ymldb-push-pull
ymldb_example_SOURCES=example.c
ymldb_example_CPPFLAGS = -I $(top_srcdir)/ymldb
ymldb_example_LDFLAGS = -L$(top_srcdir)/ymldb/.libs -L$(top_srcdir)/ycomm/.libs \
                  -lymldb -lyaml -lycomm
ymldb_example_CFLAGS = -g -Wall

ymldb_update_callback_example_SOURCES=update-callback-example.c
ymldb_update_callback_example_CPPFLAGS = -I $(top_srcdir)/ymldb
ymldb_update_callback_example_LDFLAGS = -L$(top_srcdir)/ymldb/.libs -L$(top_srcdir)/ycomm/.libs \
                  -lymldb -lyaml -lycomm
ymldb_update_callback_example_CFLAGS = -g -Wall

ymldb_file_rw_SOURCES=file-rw.c
ymldb_file_rw_CPPFLAGS = -I $(top_srcdir)/ymldb
ymldb_file_rw_LDFLAGS = -L$(top_srcdir)/ymldb/.libs -L$(top_srcdir)/ycomm/.libs \
                  -lymldb -lyaml -lycomm
ymldb_file_rw_CFLAGS = -g -Wall

ymldb_push_pull_SOURCES=push-pull.c
ymldb_push_pull_CPPFLAGS = -I $(top_srcdir)/ymldb
ymldb_push_pull_LDFLAGS = -L$(top_srcdir)/ymldb/.libs -L$(top_srcdir)/ycomm/.libs \
                  -lymldb -lyaml -lycomm
ymldb_push_pull_CFLAGS = -g -Wall