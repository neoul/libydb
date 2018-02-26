bin_PROGRAMS = ex-ytree ex-ytrie ex-yalloc ex-ylist

ex_ytree_SOURCES = ex-ytree.c
ex_ytree_CPPFLAGS = -I $(top_srcdir)/ycomm
ex_ytree_LDFLAGS = -L$(top_srcdir)/ycomm/.libs \
                  -lycomm -lyaml -lm
ex_ytree_CFLAGS = -g -Wall

ex_ylist_SOURCES = ex-ylist.c
ex_ylist_CPPFLAGS = -I $(top_srcdir)/ycomm
ex_ylist_LDFLAGS = -L$(top_srcdir)/ycomm/.libs \
    -lycomm -lyaml
ex_ylist_CFLAGS = -g -Wall

ex_ytrie_SOURCES = ex-ytrie.c
ex_ytrie_CPPFLAGS = -I $(top_srcdir)/ycomm
ex_ytrie_LDFLAGS = -L$(top_srcdir)/ycomm/.libs \
    -lycomm -lyaml
ex_ytrie_CFLAGS = -g -Wall

ex_yalloc_SOURCES = ex-yalloc.c
ex_yalloc_CPPFLAGS = -I $(top_srcdir)/ycomm
ex_yalloc_LDFLAGS = -L$(top_srcdir)/ycomm/.libs \
                  -lycomm -lyaml
ex_yalloc_CFLAGS = -g -Wall