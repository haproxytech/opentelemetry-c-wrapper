dnl am-misc.m4 by Miroslav Zagorac <mzagorac@haproxy.com>
dnl
AC_DEFUN([AX_OTEL_CPP_HDR_SEMANTIC_CONVENTIONS], [
	_ax_var_sc_file="${ac_aux_dir}ax_otel_cpp_hdr_semantic_conventions.out"

	for _var_in in $(find ${OPENTELEMETRY_INCLUDEDIR}/opentelemetry/semconv/incubating -type f); do
		dnl
		dnl escaping '[ ] $ #' here..
		dnl n - the name of the current namespace
		dnl p - namespace prefix for macro definition
		dnl k - macro definition that has data in the lines below (name)
		dnl v - the value of macro k
		dnl
		${AWK} '
			function _trim_space()	{ sub(/^[[ 	]]*/,""); sub(/[[ 	]]*$/,"") }
			function _trim()	{ sub(/;$/,""); gsub(/"/,"") }
			BEGIN			{ p=""; n=""; k=""; v="" }
			/^namespace [[^ ]]*$/	{ n=[$]2; next }
			n!=""			{ if ([$]1=="{") p=n"_"; n=""; next }
						{ _trim_space() }
			/^static constexpr int/	{ sub(/;$/,""); print "[#]define "p""[$]4"\t"[$]6; next }
			/^static constexpr.*;$/	{ sub(/;$/,""); sub("*",p,[$]5); print "[#]define "[$]5"\t"gensub(/^[[^=]]*=/,"","g"); next }
			/^static constexpr /	{ k=[$]5; sub("*",p,k); next }
			(k!="") && /;$/		{ _trim(); print "[#]define "k"\t\""v""[$]0"\""; k=""; v=""; next }
			k!=""			{ _trim(); (v=="") ? v=[$]0 : v=v""[$]0; next }
		' "${_var_in}"
	done | sort | uniq > "${_ax_var_sc_file}"

	AC_SUBST_FILE([OTEL_CPP_HDR_SEMANTIC_CONVENTIONS])
	OTEL_CPP_HDR_SEMANTIC_CONVENTIONS="${_ax_var_sc_file}"
])

AC_DEFUN([AX_OTEL_CPP_HDR_SPAN_KIND], [
	_ax_var_sk_file="${ac_aux_dir}ax_otel_cpp_hdr_span_kind.out"

	${AWK} 'BEGIN { print "typedef enum {" } /^ *Span_SpanKind_SPAN_KIND_/ { sub(/Span_SpanKind/,"OTELC",[$]0); print "\t"[$]1" "[$]2" "[$]3 } END { print "} otelc_span_kind_t;" }' ${OPENTELEMETRY_INCLUDEDIR}/opentelemetry/proto/trace/v1/trace.pb.h > "${_ax_var_sk_file}"

	AC_SUBST_FILE([OTEL_CPP_HDR_SPAN_KIND])
	OTEL_CPP_HDR_SPAN_KIND="${_ax_var_sk_file}"
])
