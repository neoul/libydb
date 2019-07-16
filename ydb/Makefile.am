lib_LTLIBRARIES = libydb.la
libydb_la_SOURCES = \
	utilities/art.c \
	utilities/utf8.c \
	utilities/base64.c \
	ylist.c \
	yarray.c \
	ytrie.c \
	ytree.c \
	ymap.c \
	ystr.c \
	ynode.c \
	ydb.c \
	ylog.c
	
libydb_la_CPPFLAGS = -Iutilities
libydb_la_CFLAGS = -g -Wall -std=c99 -D_GNU_SOURCE
libydb_la_LDFLAGS = -version-info 1:0:0
include_HEADERS = ydb.h ylist.h ytree.h ytrie.h yarray.h ymap.h ylog.h ystr.h

# if PYTHON_SWIG3
# libydb_la_SOURCES += ydb_wrap.c
# libydb_la_CPPFLAGS += -I$(includedir)/python3.6m

# swig.generation:
# 	echo "SWIG.GENERATION" 
# 	swig -python ydb.i
# ydb_wrap.c: swig.generation 
# endif