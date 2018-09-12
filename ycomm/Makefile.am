lib_LTLIBRARIES = libycomm.la

libycomm_la_SOURCES = \
    utilities/ylog.c \
    utilities/art.c \
    ylist.c \
    ytrie.c \
    ytree.c \
    yalloc.c \
    ynode.c \
    ydb.c
    
libycomm_la_CPPFLAGS = -Iutilities
libycomm_la_CFLAGS = -g -Wall -std=c99 -D_GNU_SOURCE


include_HEADERS = ynode.h ydb.h ylist.h ytree.h ytrie.h
dist_pkgdata_DATA = ../ycomm-ex.yaml
