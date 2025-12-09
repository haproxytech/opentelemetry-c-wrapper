dnl am-with-libfyaml.m4 by Miroslav Zagorac <mzagorac@haproxy.com>
dnl
AC_DEFUN([AX_WITH_LIBFYAML], [
	AC_ARG_WITH([libfyaml],
		[AS_HELP_STRING([--with-libfyaml@<:@=DIR@:>@], [use libfyaml library @<:@default=yes@:>@])],
		[],
		[with_libfyaml=check]
	)

	AX_CHECK_NOEXCEPT([])

	AS_IF(
		[test "${with_libfyaml}" != "no"],
		[
			HAVE_LIBFYAML=
			LIBFYAML_CFLAGS=
			LIBFYAML_CPPFLAGS=
			LIBFYAML_LDFLAGS=
			LIBFYAML_LIBS=
			LIBFYAML_INCLUDEDIR=

			AX_PATH_PKGCONFIG([${with_libfyaml}])
			LIBFYAML_CPPFLAGS="`PKG_CONFIG_PATH=${PKG_CONFIG_PATH} pkg-config --cflags libfyaml`"
			LIBFYAML_LDFLAGS="`PKG_CONFIG_PATH=${PKG_CONFIG_PATH} pkg-config --libs-only-L --libs-only-other libfyaml`"
			LIBFYAML_LIBS="`PKG_CONFIG_PATH=${PKG_CONFIG_PATH} pkg-config --libs-only-l libfyaml`"
			LIBFYAML_INCLUDEDIR="`PKG_CONFIG_PATH=${PKG_CONFIG_PATH} pkg-config --variable=includedir libfyaml`"

			AX_VARIABLES_STORE

			CPPFLAGS="${_saved_cppflags} ${LIBFYAML_CPPFLAGS}"
			LDFLAGS="${_saved_ldflags} ${LIBFYAML_LDFLAGS}"
			LIBS="${_saved_libs} ${LIBFYAML_LIBS}"

			AC_CHECK_HEADER(
				[libfyaml.h],
				[AC_DEFINE([HAVE_LIBFYAML_H], [1], [Define to 1 if you have the <libfyaml.h> header file.])],
				[AC_MSG_ERROR([libfyaml header file not found])],
				[]
			)
			AC_LINK_IFELSE(
				[
					AC_LANG_PROGRAM(
						[[#include <libfyaml.h>]],
						[[fy_version_default();]]
					)
				],
				[],
				[AC_MSG_ERROR([libfyaml library not found])]
			)

			HAVE_LIBFYAML=yes

			AX_VARIABLES_RESTORE

			AC_MSG_NOTICE([libfyaml environment variables:])
			AC_MSG_NOTICE([  LIBFYAML_CFLAGS=${LIBFYAML_CFLAGS}])
			AC_MSG_NOTICE([  LIBFYAML_CPPFLAGS=${LIBFYAML_CPPFLAGS}])
			AC_MSG_NOTICE([  LIBFYAML_LDFLAGS=${LIBFYAML_LDFLAGS}])
			AC_MSG_NOTICE([  LIBFYAML_LIBS=${LIBFYAML_LIBS}])
			AC_MSG_NOTICE([  LIBFYAML_INCLUDEDIR=${LIBFYAML_INCLUDEDIR}])

			AC_SUBST([LIBFYAML_CFLAGS])
			AC_SUBST([LIBFYAML_CPPFLAGS])
			AC_SUBST([LIBFYAML_LDFLAGS])
			AC_SUBST([LIBFYAML_LIBS])
			AC_SUBST([LIBFYAML_INCLUDEDIR])
		],
		[AC_MSG_NOTICE([libfyaml not used])]
	)
])
