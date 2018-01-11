bin_PROGRAMS=ymldb-test ymldb-update-callback-example ymldb-file-rw ymldb-push-pull
ymldb_test_SOURCES=ymldb-test.c
ymldb_test_CPPFLAGS = -I $(top_srcdir)/ymldb
ymldb_test_LDFLAGS = -L$(top_srcdir)/ymldb/.libs \
                  -lymldb -lcprops -lyaml
ymldb_test_CFLAGS = -g -Wall

ymldb_update_callback_example_SOURCES=ymldb-update-callback-example.c
ymldb_update_callback_example_CPPFLAGS = -I $(top_srcdir)/ymldb
ymldb_update_callback_example_LDFLAGS = -L$(top_srcdir)/ymldb/.libs \
                  -lymldb -lcprops -lyaml
ymldb_update_callback_example_CFLAGS = -g -Wall

ymldb_file_rw_SOURCES=ymldb-file-rw.c
ymldb_file_rw_CPPFLAGS = -I $(top_srcdir)/ymldb
ymldb_file_rw_LDFLAGS = -L$(top_srcdir)/ymldb/.libs \
                  -lymldb -lcprops -lyaml
ymldb_file_rw_CFLAGS = -g -Wall

ymldb_push_pull_SOURCES=push-pull.c
ymldb_push_pull_CPPFLAGS = -I $(top_srcdir)/ymldb
ymldb_push_pull_LDFLAGS = -L$(top_srcdir)/ymldb/.libs \
                  -lymldb -lcprops -lyaml
ymldb_push_pull_CFLAGS = -g -Wall