#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([zlcoroutine], [1.0], [lizhenghn@gmail.com])
AC_CONFIG_SRCDIR([src/OsDefine.h])
AC_CONFIG_HEADERS([Config.h])
AM_INIT_AUTOMAKE(zlcoroutine,1.0)
AC_PROG_LIBTOOL

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([pthread], [main])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdint.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_CHECK_FUNCS([bzero gethostbyname gettimeofday memset mkdir select socket strchr strerror])

AC_CONFIG_FILES([Makefile
                src/Makefile
                tests/Makefile
               ])

AC_OUTPUT

cp Config.h src

AC_MSG_NOTICE
AC_MSG_NOTICE([Report:])
AC_MSG_NOTICE([  zlcoroutine : 1.0 ])
AC_MSG_NOTICE([  repository  : https://github.com/lizhenghn123/zl_coroutine.git ])
AC_MSG_NOTICE([  author      : lizhenghn@gmail.com ])

