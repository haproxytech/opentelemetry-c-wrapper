dnl Process this file with autoconf to produce a configure script.
dnl
dnl configure.ac - Miroslav Zagorac <mzagorac@haproxy.com>

AC_PREREQ([2.69])
AC_COPYRIGHT([Copyright (C) 2026 HAProxy Technologies])
AC_INIT([opentelemetry-c-wrapper], m4_esyscmd_s(awk '/Package/ { print $3 }' VERSION), [mzagorac@haproxy.com], [], [https://github.com/haproxytech/opentelemetry-c-wrapper])
AC_CONFIG_MACRO_DIR([m4])
AX_VARIABLE_SET([LIB_VERSION], [m4_esyscmd_s([awk '/Library/ { print $3 }' VERSION])])
AX_VARIABLE_SET([PACKAGE_BUILD], [m4_esyscmd_s([cat src/.build-counter])])

echo "*** configuring for ${PACKAGE_NAME} v${PACKAGE_VERSION}, lib v${LIB_VERSION} ***"

AC_PREFIX_DEFAULT([/usr])
AC_CONFIG_AUX_DIR([config])
AM_INIT_AUTOMAKE([1.13])
AM_SILENT_RULES([yes])
AX_VARIABLES_INIT
AX_HOST

dnl Check for source directory.
dnl
AC_CONFIG_SRCDIR([src/tracer.cpp])
AC_CONFIG_HEADERS([config/config.h])

AX_INIT_PKGCONFIG

dnl Set the compiler, preprocessor, and file extensions.
dnl
AC_LANG([C])

dnl Checking command line options.
dnl
AX_ENABLE_DEBUG
AX_ENABLE_WARNINGS
AX_ENABLE_GPROF
AX_ENABLE_THREADS

dnl
dnl Misc
dnl

dnl Checks for programs.
dnl
AC_PROG_AWK
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

dnl Initialize libtool
dnl
LT_INIT([])
AC_SUBST([LIBTOOL_DEPS])

dnl Checks for header files.
dnl
AC_CHECK_HEADERS([fcntl.h inttypes.h malloc.h stdint.h stdlib.h string.h strings.h sys/time.h unistd.h])

dnl Checks for typedefs, structures, and compiler characteristics.
dnl
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T

dnl Checks for libraries.
dnl
AX_WITH_LIBFYAML
AX_WITH_OPENTELEMETRY

dnl Checks for library functions.
dnl
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRERROR_R
AC_FUNC_STRNLEN
AC_FUNC_STRTOD
AC_CHECK_FUNCS([clock_gettime gettimeofday mallinfo mallinfo2 memset strcasecmp strdup strndup strrchr strstr strtol])

dnl Set the compiler flags
dnl
AX_PROG_CC_SET([])
AX_PROG_CXX_SET([])
AX_CXX_COMPILE_STDCXX([17], [ext], [mandatory])

AM_CONDITIONAL([WANT_DEBUG], [test "${enable_debug}" = "yes"])

AX_VARIABLES_SET

AX_VARIABLE_SET([OPENTELEMETRY_C_WRAPPER_CPPFLAGS], [ ${THREADS_CPPFLAGS} ${LIBFYAML_CPPFLAGS} ])
AX_VARIABLE_SET([OPENTELEMETRY_C_WRAPPER_CFLAGS],   [ ${THREADS_CFLAGS}   ${LIBFYAML_CFLAGS}   ])
AX_VARIABLE_SET([OPENTELEMETRY_C_WRAPPER_LDFLAGS],  [ ${THREADS_LDFLAGS}  ${LIBFYAML_LDFLAGS}  ])
AX_VARIABLE_SET([OPENTELEMETRY_C_WRAPPER_LIBS],     [ ${THREADS_LIBS}     ${LIBFYAML_LIBS}     ])

AC_SUBST([OPENTELEMETRY_C_WRAPPER_CPPFLAGS])
AC_SUBST([OPENTELEMETRY_C_WRAPPER_CFLAGS])
AC_SUBST([OPENTELEMETRY_C_WRAPPER_LDFLAGS])
AC_SUBST([OPENTELEMETRY_C_WRAPPER_LIBS])

dnl version.h
dnl
CONFIGURE_OPTIONS="${ac_configure_args}"
AC_SUBST([DATE])
AC_SUBST([PACKAGE_VERSION])
AC_SUBST([PACKAGE_BUILD])
AC_SUBST([LIB_VERSION])
AC_SUBST([CONFIGURE_OPTIONS])

dnl src/Makefile test/Makefile
dnl
COMMON_CPPFLAGS='-I$(top_builddir)/include -I$(top_srcdir)/include'
AC_SUBST([COMMON_CPPFLAGS])

dnl otel_cpp.h
dnl
AX_OTEL_CPP_HDR_SEMANTIC_CONVENTIONS
AX_OTEL_CPP_HDR_SPAN_KIND

AC_CONFIG_FILES([Makefile src/Makefile test/Makefile])
AC_CONFIG_FILES([include/opentelemetry-c-wrapper/version.h include/opentelemetry-c-wrapper/otel_cpp.h])
AC_CONFIG_FILES([opentelemetry-c-wrapper.pc opentelemetry-c-wrapper_dbg.pc])
AC_OUTPUT

AX_SHOW_CONFIG


AH_TOP([
#ifndef _COMMON_CONFIG_H
#define _COMMON_CONFIG_H
])

AH_BOTTOM([
#endif /* _COMMON_CONFIG_H */
])
