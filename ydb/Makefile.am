lib_LTLIBRARIES = libydb.la
libydb_la_SOURCES = \
    utilities/art.c \
    ylist.c \
    yarray.c \
    ytrie.c \
    ytree.c \
    ymap.c \
    yalloc.c \
    ynode.c \
    ydb.c \
    yipc.c \
    ylog.c
    
libydb_la_CPPFLAGS = -Iutilities
libydb_la_CFLAGS = -g -Wall -std=c99 -D_GNU_SOURCE
libydb_la_LDFLAGS = -version-info 1:0:0
include_HEADERS = ynode.h ydb.h ylist.h ytree.h ytrie.h yarray.h ymap.h yipc.h ylog.h
