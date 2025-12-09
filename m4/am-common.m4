dnl am-common.m4 by Miroslav Zagorac <mzagorac@haproxy.com>
dnl
AC_DEFUN([AX_TYPE_BOOL_T], [AC_CHECK_TYPE([bool_t], [unsigned char])])
AC_DEFUN([AX_TYPE_UNCHAR], [AC_CHECK_TYPE([unchar], [unsigned char])])
AC_DEFUN([AX_TYPE_USHORT], [AC_CHECK_TYPE([ushort], [unsigned short])])
AC_DEFUN([AX_TYPE_ULONG],  [AC_CHECK_TYPE([ulong],  [unsigned long])])
AC_DEFUN([AX_TYPE_UINT],   [AC_CHECK_TYPE([uint],   [unsigned int])])

AC_DEFUN([AX_VARIABLES_STORE], [
	_saved_cppflags="${CPPFLAGS}"
	_saved_cflags="${CFLAGS}"
	_saved_cxxflags="${CXXFLAGS}"
	_saved_ldflags="${LDFLAGS}"
	_saved_libs="${LIBS}"
])

AC_DEFUN([AX_VARIABLES_RESTORE], [
	CPPFLAGS="${_saved_cppflags}"
	CFLAGS="${_saved_cflags}"
	CXXFLAGS="${_saved_cxxflags}"
	LDFLAGS="${_saved_ldflags}"
	LIBS="${_saved_libs}"
])

AC_DEFUN([AX_VARIABLES_INIT], [
	SET_CPPFLAGS=
	SET_CFLAGS=
	SET_CXXFLAGS=
	SET_LDFLAGS=
	SET_LIBS=
])

AC_DEFUN([AX_VARIABLES_SET], [
	CPPFLAGS="${CPPFLAGS} ${SET_CPPFLAGS}"
	CFLAGS="${CFLAGS} ${SET_CFLAGS}"
	CXXFLAGS="${CXXFLAGS} ${SET_CXXFLAGS}"
	LDFLAGS="${LDFLAGS} ${SET_LDFLAGS}"
	LIBS="${LIBS} ${SET_LIBS}"
])

dnl Check which options the C compiler supports.
dnl
AC_DEFUN([AX_PROG_CC_SET], [
	_var_cflags=
	_var_tmp_cflags=
	_loop_cflags=

	AX_VARIABLES_STORE

	AS_CASE(
		[${CC}],
		[gcc*|clang*], [
			_var_cflags="\
				-Werror=unknown-warning-option \
				-Wall \
				-Wextra"
			AS_IF(
				[test "${enable_warnings}" = "yes"],
				[
					_var_cflags="${_var_cflags} \
						-Walloca \
						-Waddress-of-packed-member \
						-Waggregate-return \
						-Wbad-function-cast \
						-Wcast-align \
						-Wdiscarded-qualifiers \
						-Werror=nonnull \
						-Wfloat-equal \
						-Winline \
						-Wmissing-declarations \
						-Wmissing-noreturn \
						-Wmissing-prototypes \
						-Wnested-externs \
						-Wold-style-definition \
						-Wpointer-arith \
						-Wredundant-decls \
						-Wshadow \
						-Wstrict-prototypes \
						-Wundef \
						-Wunused \
						-Wvla \
						-Wwrite-strings"

					AS_IF(
						[test "${enable_debug}" = "yes"],
						[
							_var_cflags="${_var_cflags} \
								-Wformat=2"
						],
						[
							_var_cflags="${_var_cflags} \
								-Wformat-security \
								-Wformat-y2k"
						]
					)
				],
				[
					_var_cflags="${_var_cflags} \
						-Werror"
				]
			)
		],
		[cc], [
			AS_CASE(
				[${host_os}],
				[*solaris*], [_var_cflags="-xCC"],
				[*freebsd1?.*], [_var_cflags=],
				[]
			)
		],
		[]
	)

	# Do not replace $n with ${n} here, as these refer to the m4 arguments
	# of the AX_PROG_CC_SET function.
	#
	_var_tmp_cflags="${CFLAGS}"
	CFLAGS=
	AC_LANG_PUSH([C])
	for _loop_cflags in ${_var_cflags} $1; do
		AX_CHECK_COMPILE_FLAG(
			[${_loop_cflags}],
			[CFLAGS="${CFLAGS} ${_loop_cflags}"]
		)
	done
	AC_LANG_POP([C])
	SET_CFLAGS="${SET_CFLAGS} ${CFLAGS}"
	CFLAGS="${_var_tmp_cflags}"

	AX_VARIABLES_RESTORE
])

dnl Check which options the C++ compiler supports.
dnl
AC_DEFUN([AX_PROG_CXX_SET], [
	_var_cxxflags=
	_loop_cxxflags=
	_var_tmp_cxxflags=

	AX_VARIABLES_STORE

	AS_CASE(
		[${CXX}],
		[g++*|clang++*], [
			_var_cxxflags="\
				-Werror=unknown-warning-option \
				-Wall \
				-Wextra"
			AS_IF(
				[test "${enable_warnings}" = "yes"],
				[
					_var_cxxflags="${_var_cxxflags} \
						-Walloca \
						-Waddress-of-packed-member \
						-Wcast-align \
						-Werror=nonnull \
						-Wfloat-equal \
						-Winline \
						-Wmissing-declarations \
						-Wmissing-noreturn \
						-Wpointer-arith \
						-Wundef \
						-Wunused \
						-Wvla \
						-Wwrite-strings \
						\
						-Wno-invalid-offsetof \
						-Wno-terminate \
						-Wnoexcept \
						-Woverloaded-virtual \
						-Wsign-promo \
						-Wsized-deallocation \
						-Wstrict-null-sentinel \
						-Wvirtual-inheritance"

					AS_IF(
						[test "${enable_debug}" = "yes"],
						[
							_var_cxxflags="${_var_cxxflags} \
								-Waggregate-return \
								-Wconditionally-supported \
								-Wformat=2 \
								-Wmultiple-inheritance \
								-Wnamespaces \
								-Wold-style-cast \
								-Wredundant-decls \
								-Wshadow \
								-Wtemplates \
								-Wuseless-cast \
								-Wzero-as-null-pointer-constant"
						],
						[
							_var_cxxflags="${_var_cxxflags} \
								-Wformat-security \
								-Wformat-y2k \
								-ftemplate-backtrace-limit=0"
						]
					)
				],
				[
					_var_cxxflags="${_var_cxxflags} \
						-Werror"
				]
			)
		],
		[c++], [
			AS_CASE([${host_os}], [*freebsd1?.*], [_var_cxxflags="-Wno-extern-c-compat"], [])
		],
		[CC], [
			AS_CASE([${host_os}], [*solaris*], [_var_cxxflags=], [])
		],
		[]
	)

	# Do not replace $n with ${n} here, as these refer to the m4 arguments
	# of the AX_PROG_CXX_SET function.
	#
	_var_tmp_cxxflags="${CXXFLAGS}"
	CXXFLAGS=
	AC_LANG_PUSH([C++])
	for _loop_cxxflags in ${_var_cxxflags} $1; do
		AX_CHECK_COMPILE_FLAG(
			[${_loop_cxxflags}],
			[CXXFLAGS="${CXXFLAGS} ${_loop_cxxflags}"]
		)
	done
	AC_LANG_POP([C++])
	SET_CXXFLAGS="${SET_CXXFLAGS} ${CXXFLAGS}"
	CXXFLAGS="${_var_tmp_cxxflags}"

	AX_VARIABLES_RESTORE
])

dnl Check whether the C++ compiler has noexcept specifier.
dnl
AC_DEFUN([AX_CHECK_NOEXCEPT], [
	AC_MSG_CHECKING([whether the C++ compiler (${CXX}) has noexcept specifier])
	AC_LANG_PUSH([C++])
	AC_COMPILE_IFELSE(
		[
			AC_LANG_PROGRAM(
				[],
				[void (*fp)() noexcept(false);]
			)
		],
		[AC_MSG_RESULT([ yes])],
		[
			CXXFLAGS="-std=gnu++11"
			AC_MSG_RESULT([ no])
		]
	)
	AC_LANG_POP([C++])
])

dnl Check whether the C compiler has __DATE__ macro.
dnl
AC_DEFUN([AX_CHECK___DATE__], [
	AC_MSG_CHECKING([whether the C compiler (${CC}) has __DATE__ macro])
	AC_COMPILE_IFELSE(
		[
			AC_LANG_PROGRAM(
				[],
				[char *test=__DATE__;]
			)
		],
		[AC_MSG_RESULT([ yes])],
		[
			AC_DEFINE_UNQUOTED([__DATE__], ["`date`"], [Define if your C compiled doesn't have __DATE__ macro.])
			AC_MSG_RESULT([ no])
		]
	)
])

dnl Check whether the C compiler has __func__ variable.
dnl
AC_DEFUN([AX_CHECK___FUNC__], [
	AC_MSG_CHECKING([whether the C compiler (${CC}) has __func__ variable])
	AC_COMPILE_IFELSE(
		[
			AC_LANG_PROGRAM(
				[#include <stdio.h>],
				[printf ("%s", __func__);]
			)
		],
		[AC_MSG_RESULT([ yes])],
		[
			AC_DEFINE_UNQUOTED([__func__], ["__unknown__"], [Define if your C compiler doesn't have __func__ variable.])
			AC_MSG_RESULT([ no])
		]
	)
])

dnl Check whether the C compiler defines __STDC__.
dnl
AC_DEFUN([AX_CHECK___STDC__], [
	AC_MSG_CHECKING([whether the C compiler (${CC}) defines __STDC__])
	AC_COMPILE_IFELSE(
		[
			AC_LANG_PROGRAM(
				[],
				[
					#ifndef __STDC__
					test_stdc ();
					#endif
				]
			)
		],
		[
			AC_MSG_RESULT([ yes])
			AC_DEFINE_UNQUOTED([ANSI_FUNC], [1], [Define if you use an ANSI C compiler.])
			stdc_defined="yes"
		],
		[AC_MSG_RESULT([ no])]
	)
])

dnl
dnl
AC_DEFUN([AX_INIT_PKGCONFIG], [
	PKG_PROG_PKG_CONFIG([])
	PKG_INSTALLDIR([])

	AC_SUBST([PKG_CONFIG_PATH_SAVE], [${PKG_CONFIG_PATH}])
	export PKG_CONFIG_PATH
])

dnl
dnl
AC_DEFUN([AX_PATH_PKGCONFIG], [
	_pc_prefix="${ac_default_prefix}"

	PKG_CONFIG_PATH="${PKG_CONFIG_PATH_SAVE}"

	# Do not replace $n with ${n} here, as these refer to the m4 arguments
	# of the AX_PATH_PKGCONFIG function.
	#
	AS_IF([test "$1" = "yes" -o "$1" = "check"], [], [_pc_prefix="$1"])

	for _loop_path in \
		${_pc_prefix}/lib \
		${_pc_prefix}/lib/i386-linux-gnu \
		${_pc_prefix}/lib/x86_64-linux-gnu \
		${_pc_prefix}/amd64 \
		${_pc_prefix}/lib32 \
		${_pc_prefix}/lib64 \
		${_pc_prefix}/share \
		${prefix}/lib
	do
		AS_IF(
			[test -d "${_loop_path}/pkgconfig"],
			[PKG_CONFIG_PATH="${PKG_CONFIG_PATH}${PKG_CONFIG_PATH:+:}${_loop_path}/pkgconfig"],
			[]
		)
	done
	AC_MSG_NOTICE([PKG_CONFIG_PATH=${PKG_CONFIG_PATH}])
])

dnl Check whether the C compiler has __attribute__ keyword.
dnl
AC_DEFUN([AX_CHECK___ATTRIBUTE__], [
	AC_MSG_CHECKING([whether the C compiler (${CC}) has __attribute__ keyword])
	AC_COMPILE_IFELSE(
		[
			AC_LANG_PROGRAM(
				[void t1 () __attribute__ ((noreturn)); void t1 () { return; };],
				[t1 ();]
			)
		],
		[
			AC_MSG_RESULT([ yes])
			AC_DEFINE_UNQUOTED([__ATTRIBUTE__], [1], [Define if your C compiler has __attribute__ keyword.])
		],
		[AC_MSG_RESULT([ no])]
	)
])

AC_DEFUN([AX_ENABLE_DEBUG], [
	AC_ARG_ENABLE(
		[debug],
		[AS_HELP_STRING([--enable-debug], [compile with debugging symbols/functions])],
		[
			AS_IF(
				[test "${enableval}" = "yes"],
				[
					AC_DEFINE([DEBUG], [1], [Define to 1 if you want to include debugging options.])
					CFLAGS="${CFLAGS} -g -O0"
					CXXFLAGS="${CXXFLAGS} -g -O0"
				],
				[]
			)
		]
	)
])

AC_DEFUN([AX_ENABLE_WARNINGS], [
	AC_ARG_ENABLE(
		[warnings],
		[AS_HELP_STRING([--enable-warnings], [enable additional compiler warnings by adding extra CFLAGS/CXXFLAGS])],
		[]
	)
])

AC_DEFUN([AX_ENABLE_GPROF], [
	AC_ARG_ENABLE(
		[gprof],
		[AS_HELP_STRING([--enable-gprof], [enable profiling with gprof])],
		[
			AS_IF(
				[test "${enableval}" = "yes"],
				[
					AC_DEFINE([GPROF], [1], [Define to 1 if you want to enable profiling with gprof.])
					CFLAGS="${CFLAGS} -pg"
					CXXFLAGS="${CXXFLAGS} -pg"
					LDFLAGS="${LDFLAGS} -pg"
				],
				[]
			)
		]
	)
])

AC_DEFUN([AX_VARIABLE_SET], [
_am_cache_test ()
{
	_c=

	AS_IF(
		[test -n "${2}"],
		[
			for _c in ${2}; do test "${_c}" = "${3}" && return 1; done
			eval "${1}=\"${2} ${3}\""
		],
		[eval "${1}=\"${3}\""]
	)
}

	# Do not replace $n with ${n} here, as these refer to the m4 arguments
	# of the AX_VARIABLE_SET function.
	#
	_am_var_resolved=
	for _am_var_loop in $2; do
		_am_cache_test _am_var_resolved "${_am_var_resolved}" "${_am_var_loop}"
	done
	$1=${_am_var_resolved}
])

AC_DEFUN([AX_SHOW_CONFIG], [
	eval "bindir=${bindir}"
	eval "datadir=${datadir}"
	eval "sysconfdir=${sysconfdir}"
	eval "mandir=${mandir}"

	echo
	echo "${PACKAGE_NAME} configuration:"
	echo "--------------------------------------------------"
	echo "  package version         : ${PACKAGE_VERSION}"
	echo "  library version         : ${LIB_VERSION}"
	echo "  host operating system   : ${host}"
	echo "  source code location    : ${srcdir}"
	echo "  C compiler              : ${CC}"
	echo "  C++ compiler            : ${CXX}"
	echo "  preprocessor flags      : ${CPPFLAGS}"
	echo "  C compiler flags        : ${CFLAGS}"
	echo "  C++ compiler flags      : ${CXXFLAGS}"
	echo "  linker flags            : ${LDFLAGS}"
	echo "  libraries               : ${LIBS}"
	echo "  configure options       : ${ac_configure_args}"
	echo "  binary install path     : ${bindir}"
	echo "  data install path       : ${datadir}"
	echo "  configuration file path : ${sysconfdir}"
	echo "  man page install path   : ${mandir}"
	echo
])
