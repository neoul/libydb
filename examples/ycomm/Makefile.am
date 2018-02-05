bin_PROGRAMS = ex-ytree ex-yque ex-ytrie ex-yalloc

ex_ytree_SOURCES = ex-ytree.c
ex_ytree_CPPFLAGS = -I $(top_srcdir)/ycomm
ex_ytree_LDFLAGS = -L$(top_srcdir)/ycomm/.libs \
                  -lycomm -lyaml -lm
ex_ytree_CFLAGS = -g -Wall

ex_yque_SOURCES = ex-yque.c
ex_yque_CPPFLAGS = -I $(top_srcdir)/ycomm -I $(top_srcdir)/ycomm/utilities
ex_yque_LDFLAGS = -L$(top_srcdir)/ycomm/.libs \
    -lycomm -lyaml
ex_yque_CFLAGS = -g -Wall

ex_ytrie_SOURCES = ex-ytrie.c
ex_ytrie_CPPFLAGS = -I $(top_srcdir)/ycomm -I $(top_srcdir)/ycomm/utilities
ex_ytrie_LDFLAGS = -L$(top_srcdir)/ycomm/.libs \
    -lycomm -lyaml
ex_ytrie_CFLAGS = -g -Wall

ex_yalloc_SOURCES = ex-yalloc.c
ex_yalloc_CPPFLAGS = -I $(top_srcdir)/ycomm
ex_yalloc_LDFLAGS = -L$(top_srcdir)/ycomm/.libs \
                  -lycomm -lyaml -lm
ex_yalloc_CFLAGS = -g -Wall