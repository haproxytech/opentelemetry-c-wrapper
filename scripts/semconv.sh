#!/bin/sh -u
#
# Extract OpenTelemetry semantic convention definitions from C++ header files
# and convert them into C preprocessor #define macros.  The output is assembled
# using the semconv.h.in template, with the @SEMANTIC_CONVENTIONS@ placeholder
# replaced by the generated macro definitions.
#
OPENTELEMETRY_INCLUDEDIR="${1:-/opt/include}"
                     AWK="awk"

	_ax_var_sc_src="${OPENTELEMETRY_INCLUDEDIR}/opentelemetry/semconv/incubating"
	_ax_var_sc_out="$(dirname "${0}")/semconv.h"
	_ax_var_sc_tmp="${_ax_var_sc_out}.tmp"
	_ax_var_sc_in="${_ax_var_sc_out}.in"

	rm -f "${_ax_var_sc_tmp}"
	${AWK} 'BEGIN { RS="@SEMANTIC_CONVENTIONS@" } NR==1 { print }' "${_ax_var_sc_in}" > "${_ax_var_sc_out}"
	for _var_in in "${_ax_var_sc_src}"/*; do
#		dnl
#		dnl escaping '[ ] $ #' here..
#		dnl n - the name of the current namespace
#		dnl p - namespace prefix for macro definition
#		dnl k - macro definition that has data in the lines below (name)
#		dnl v - the value of macro k
#		dnl
		${AWK} '
			function _trim_space()	{ sub(/^[ 	]*/,""); sub(/[ 	]*$/,"") }
			function _trim()	{ sub(/;$/,""); gsub(/"/,"") }
			BEGIN			{ p=""; n=""; k=""; v="" }
			/^namespace [^ ]*$/	{ n=$2; next }
			n!=""			{ if ($1=="{") p=n"_"; n=""; next }
						{ _trim_space() }
			/^static constexpr int/	{ sub(/;$/,""); print "#define "p""$4"\t"$6; next }
			/^static constexpr.*;$/	{ sub(/;$/,""); sub("*",p,$5); s=$0; sub(/^[^=]*=/,"",s); print "#define "$5"\t"s; next }
			/^static constexpr /	{ k=$5; sub("*",p,k); next }
			(k!="") && /;$/		{ _trim(); print "#define "k"\t\""v""$0"\""; k=""; v=""; next }
			k!=""			{ _trim(); (v=="") ? v=$0 : v=v""$0; next }
		' "${_var_in}" >> "${_ax_var_sc_tmp}"
	done
	sort "${_ax_var_sc_tmp}" | uniq >> "${_ax_var_sc_out}"
	${AWK} 'BEGIN { RS="@SEMANTIC_CONVENTIONS@" } NR==2 { print }' "${_ax_var_sc_in}" >> "${_ax_var_sc_out}"
