#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([libymldb], [0.1], [neoul@actusnetworks.com])
AM_INIT_AUTOMAKE(libymldb, 0.1)
#AC_CONFIG_SRCDIR([test/standalone/ymldb-main.c])
AC_CONFIG_SRCDIR([ymldb/ymldb.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset mkfifo select socket strchr strdup strerror strstr])

AC_CONFIG_FILES([Makefile
                 test/publisher/Makefile
                 test/standalone/Makefile
                 test/subscriber/Makefile
                 ymldb/Makefile])
AC_OUTPUT
