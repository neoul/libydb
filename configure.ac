#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
m4_define([libydb_major_version], [1])
m4_define([libydb_minor_version], [0])
m4_define([libydb_micro_version], [66])
m4_define([libydb_version],
      [libydb_major_version.libydb_minor_version.libydb_micro_version])

AC_PREREQ([2.69])
AC_INIT([libydb], [libydb_version], [neoul@ymail.com])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
# AC_PROG_LIBTOOL

LT_INIT

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdio.h time.h stdlib.h string.h stdint.h assert.h ctype.h \
      stdarg.h unistd.h yaml.h sys/socket.h sys/un.h errno.h sys/epoll.h pthread.h \
      sys/timerfd.h stdbool.h signal.h sys/types.h yaml.h],,AC_MSG_ERROR([Could not find certain include header files]),)

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset mkfifo select socket strchr strdup strerror strstr])

# AC_ARG_ENABLE([python3], AS_HELP_STRING([--enable-python3], [Enable Python3 SWIG (Simplified Wrapper and Interface Generator)]))
# AM_CONDITIONAL([PYTHON_SWIG3], [test "x$enable_python3" = "xyes"])
# AS_IF([test "x$enable_python3" = "xyes"], [
#       # AC_CHECK_HEADERS([Python.h]),
#       AC_DEFINE(PYTHON_SWIG3, 1, "Enable YDB SWIG interface for python")
# ])

AC_ARG_ENABLE(examples, [  --enable-examples      Enable YDB examples])
AM_CONDITIONAL([HAVE_EXAMPLES], [test "x$enable_examples" = "xyes"])
if test "$enable_examples" = "yes"; then
  AC_DEFINE(HAVE_EXAMPLES,, Enable YDB examples)
fi

AC_CHECK_HEADERS([plog.h], [AC_SUBST(CFLAGS, "$CFLAGS -DENABLED_PLOG=1 -lplog")], [],[])

AM_INIT_AUTOMAKE([subdir-objects])

AC_CONFIG_FILES([Makefile
      examples/ydb/Makefile
      tools/ydb/Makefile
      ydb/Makefile])
AC_OUTPUT
