lib_LTLIBRARIES = libydb.la

libydb_la_SOURCES = \
    utilities/ylog.c \
    utilities/art.c \
    ylist.c \
    yarray.c \
    ytrie.c \
    ytree.c \
    yalloc.c \
    ynode.c \
    ydb.c
    
libydb_la_CPPFLAGS = -Iutilities
libydb_la_CFLAGS = -g -Wall -std=c99 -D_GNU_SOURCE


include_HEADERS = ynode.h ydb.h ylist.h ytree.h ytrie.h yarray.h
# dist_pkgdata_DATA = ../ydb-ex.yaml
