dnl am-with-rapidyaml.m4 by Miroslav Zagorac <mzagorac@haproxy.com>
dnl
AC_DEFUN([AX_WITH_RAPIDYAML], [
	AS_IF(
		[test "${with_rapidyaml}" != "no"],
		[
			HAVE_RAPIDYAML=
			RAPIDYAML_CXXFLAGS=
			RAPIDYAML_CPPFLAGS=
			RAPIDYAML_LDFLAGS=
			RAPIDYAML_LIBS=
			RAPIDYAML_INCLUDEDIR=

			AS_IF(
				[test "${with_rapidyaml}" != "yes" && test "${with_rapidyaml}" != "check"],
				[
					RAPIDYAML_CPPFLAGS="-I${with_rapidyaml}/include"
					RAPIDYAML_LDFLAGS="-L${with_rapidyaml}/lib"
				],
				[
					AS_IF(
						[test "${prefix}" != "NONE"],
						[
							RAPIDYAML_CPPFLAGS="-I${prefix}/include"
							RAPIDYAML_LDFLAGS="-L${prefix}/lib"
						],
						[]
					)
				]
			)

			AX_VARIABLES_STORE

			CPPFLAGS="${_saved_cppflags} ${RAPIDYAML_CPPFLAGS}"
			LDFLAGS="${_saved_ldflags} ${RAPIDYAML_LDFLAGS}"
			LIBS="${_saved_libs} ${RAPIDYAML_LIBS} -lryml"

			AC_LANG_PUSH([C++])

			AC_CHECK_HEADER(
				[ryml_std.hpp],
				[AC_DEFINE([HAVE_RYML_STD_HPP], [1], [Define to 1 if you have the <ryml_std.hpp> header file.])],
				[AC_MSG_ERROR([Rapid YAML header file not found])],
				[]
			)
			AC_LINK_IFELSE(
				[
					AC_LANG_PROGRAM(
						[[#include <ryml.hpp>] [#include <ryml_std.hpp>]],
						[[ryml::Tree tree;]]
					)
				],
				[
					RAPIDYAML_LIBS="${RAPIDYAML_LIBS} -lryml"
					HAVE_RAPIDYAML=yes
				],
				[AC_MSG_ERROR([Rapid YAML library not found])]
			)

			AC_LANG_POP([C++])

			AX_VARIABLES_RESTORE

			AC_MSG_NOTICE([Rapid YAML environment variables:])
			AC_MSG_NOTICE([  RAPIDYAML_CXXFLAGS=${RAPIDYAML_CXXFLAGS}])
			AC_MSG_NOTICE([  RAPIDYAML_CPPFLAGS=${RAPIDYAML_CPPFLAGS}])
			AC_MSG_NOTICE([  RAPIDYAML_LDFLAGS=${RAPIDYAML_LDFLAGS}])
			AC_MSG_NOTICE([  RAPIDYAML_LIBS=${RAPIDYAML_LIBS}])
			AC_MSG_NOTICE([  RAPIDYAML_INCLUDEDIR=${RAPIDYAML_INCLUDEDIR}])

			AC_SUBST([RAPIDYAML_CXXFLAGS])
			AC_SUBST([RAPIDYAML_CPPFLAGS])
			AC_SUBST([RAPIDYAML_LDFLAGS])
			AC_SUBST([RAPIDYAML_LIBS])
			AC_SUBST([RAPIDYAML_INCLUDEDIR])
		],
		[AC_MSG_NOTICE([Rapid YAML not used])]
	)
])
