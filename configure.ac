#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
m4_define([libydb_major_version], [1])
m4_define([libydb_minor_version], [0])
m4_define([libydb_micro_version], [18])
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
      stdarg.h unistd.h yaml.h sys/socket.h sys/un.h errno.h sys/epoll.h \
      sys/timerfd.h stdbool.h signal.h sys/types.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset mkfifo select socket strchr strdup strerror strstr])

AM_INIT_AUTOMAKE([subdir-objects])

AC_CONFIG_FILES([Makefile
                 examples/ydb/Makefile
                 tools/ydb/Makefile
                 ydb/Makefile])
AC_OUTPUT
