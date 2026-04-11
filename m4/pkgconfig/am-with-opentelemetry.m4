dnl am-with-opentelemetry.m4 by Miroslav Zagorac <mzagorac@haproxy.com>
dnl
AC_DEFUN([AX_WITH_OPENTELEMETRY], [
	AC_ARG_WITH([opentelemetry],
		[AS_HELP_STRING([--with-opentelemetry@<:@=DIR@:>@], [use OpenTelemetry library @<:@default=yes@:>@])],
		[],
		[with_opentelemetry=check]
	)

	AX_CHECK_NOEXCEPT([])

	AS_IF([test "${with_opentelemetry}" != "no"], [
		HAVE_OPENTELEMETRY=
		OPENTELEMETRY_CFLAGS=
		OPENTELEMETRY_CXXFLAGS=
		OPENTELEMETRY_CPPFLAGS="-DOPENTELEMETRY_PROTO_API= "
		OPENTELEMETRY_LDFLAGS=
		OPENTELEMETRY_LIBS=
		OPENTELEMETRY_INCLUDEDIR=

		AX_PATH_PKGCONFIG([${with_opentelemetry}])
		OPENTELEMETRY_CPPFLAGS="${OPENTELEMETRY_CPPFLAGS}`PKG_CONFIG_PATH=${PKG_CONFIG_PATH} pkg-config --cflags opentelemetry_api`"
		OPENTELEMETRY_LDFLAGS="`PKG_CONFIG_PATH=${PKG_CONFIG_PATH} pkg-config --libs-only-L --libs-only-other opentelemetry_api`"
		OPENTELEMETRY_LIBS="`PKG_CONFIG_PATH=${PKG_CONFIG_PATH} pkg-config --libs-only-l opentelemetry_api`"
		OPENTELEMETRY_INCLUDEDIR="`PKG_CONFIG_PATH=${PKG_CONFIG_PATH} pkg-config --variable=includedir opentelemetry_api`"

		AS_IF(
			[PKG_CONFIG_PATH=${PKG_CONFIG_PATH} pkg-config --atleast-version=1.26.0 opentelemetry_api],
			[AC_MSG_NOTICE([*** OpenTelemetry: opentelemetry_api >= 1.26.0 found])],
			[AC_MSG_ERROR([*** OpenTelemetry: opentelemetry_api >= 1.26.0 not found])]
		)

		AX_VARIABLES_STORE

		AC_LANG_PUSH([C++])
		CPPFLAGS="${_saved_cppflags} ${OPENTELEMETRY_CPPFLAGS}"
		LDFLAGS="${_saved_ldflags} ${OPENTELEMETRY_LDFLAGS}"
		LIBS="${_saved_libs} ${OPENTELEMETRY_LIBS}"

		AC_CHECK_HEADER([opentelemetry/version.h], [], [AC_MSG_ERROR([OpenTelemetry library headers not found])], [])
		AC_COMPUTE_INT(
			[_otel_abi_version],
			[OPENTELEMETRY_ABI_VERSION_NO],
			[],
			[#include <opentelemetry/version.h>]
		)
		AS_IF(
			[test "${_otel_abi_version}" -eq 2],
			[AC_MSG_NOTICE([*** OpenTelemetry: ABI version is 2])],
			[AC_MSG_ERROR([*** OpenTelemetry: Invalid ABI version: expected 2, got ${_otel_abi_version}])]
		)

		AC_RUN_IFELSE([
			AC_LANG_PROGRAM(
				[[#include <stdio.h>]
				 [#include <opentelemetry/version.h>]],
				[[fprintf(stderr, "%s", OPENTELEMETRY_VERSION);]]
			)],
			[OTELCPP_VERSION=$(./conftest$ac_exeext 2>&1)],
			[AC_MSG_ERROR(Failed to compile test program)],
			[]
		)
		AC_SUBST([OTELCPP_VERSION])
		AC_LINK_IFELSE([
			AC_LANG_PROGRAM(
				[[#include <opentelemetry/trace/provider.h>]],
				[[std::shared_ptr<OPENTELEMETRY_NAMESPACE::trace::TracerProvider> p;] [OPENTELEMETRY_NAMESPACE::trace::Provider::SetTracerProvider(p);]]
			)],
			[],
			[AC_MSG_ERROR([OpenTelemetry library not found])]
		)

		AC_MSG_NOTICE([*** OpenTelemetry exporters ***])

		AC_MSG_CHECKING([for Elasticsearch exporter])
		LIBS="${_saved_libs} ${OPENTELEMETRY_LIBS} -lopentelemetry_exporter_elasticsearch_logs"
		AC_LINK_IFELSE([
			AC_LANG_PROGRAM(
				[[#include <opentelemetry/exporters/elasticsearch/es_log_record_exporter.h>]],
				[[std::shared_ptr<OPENTELEMETRY_NAMESPACE::exporter::logs::ElasticsearchLogRecordExporter> c;] [auto r = c->MakeRecordable();]]
			)], [
				AC_MSG_RESULT([yes])
				AC_DEFINE([HAVE_OTEL_EXPORTER_ELASTICSEARCH], [1], [Have OpenTelemetry Elasticsearch exporter.])
				OPENTELEMETRY_LIBS="${OPENTELEMETRY_LIBS} -lopentelemetry_exporter_elasticsearch_logs"
			], [AC_MSG_RESULT([no])]
		)

		AC_MSG_CHECKING([for In-Memory exporter])
		LIBS="${_saved_libs} ${OPENTELEMETRY_LIBS} -lopentelemetry_exporter_in_memory -lopentelemetry_exporter_in_memory_metric"
		AC_LINK_IFELSE([
			AC_LANG_PROGRAM(
				[[#include <opentelemetry/exporters/memory/in_memory_span_exporter_factory.h>]],
				[[std::shared_ptr<OPENTELEMETRY_NAMESPACE::exporter::memory::InMemorySpanData> c;] [OPENTELEMETRY_NAMESPACE::exporter::memory::InMemorySpanExporterFactory::Create(c);]]
			)], [
				AC_MSG_RESULT([yes])
				AC_DEFINE([HAVE_OTEL_EXPORTER_IN_MEMORY], [1], [Have OpenTelemetry In-Memory exporter.])
				OPENTELEMETRY_LIBS="${OPENTELEMETRY_LIBS} -lopentelemetry_exporter_in_memory -lopentelemetry_exporter_in_memory_metric"
			], [AC_MSG_RESULT([no])]
		)

		AC_MSG_CHECKING([for Ostream exporter])
		LIBS="${_saved_libs} ${OPENTELEMETRY_LIBS} -lopentelemetry_exporter_ostream_span -lopentelemetry_exporter_ostream_metrics -lopentelemetry_exporter_ostream_logs"
		AC_LINK_IFELSE([
			AC_LANG_PROGRAM(
				[[#include <opentelemetry/exporters/ostream/span_exporter.h>]
				 [#include <opentelemetry/exporters/ostream/span_exporter_factory.h>]],
				[[OPENTELEMETRY_NAMESPACE::exporter::trace::OStreamSpanExporterFactory::Create();]]
			)], [
				AC_MSG_RESULT([yes])
				AC_DEFINE([HAVE_OTEL_EXPORTER_OSTREAM], [1], [Have OpenTelemetry Ostream exporter.])
				OPENTELEMETRY_LIBS="${OPENTELEMETRY_LIBS} -lopentelemetry_exporter_ostream_span -lopentelemetry_exporter_ostream_metrics -lopentelemetry_exporter_ostream_logs"
			], [AC_MSG_RESULT([no])]
		)

		AC_MSG_CHECKING([for OTLP/File exporter])
		LIBS="${_saved_libs} ${OPENTELEMETRY_LIBS} -lopentelemetry_exporter_otlp_file -lopentelemetry_exporter_otlp_file_metric -lopentelemetry_exporter_otlp_file_log"
		AC_LINK_IFELSE([
			AC_LANG_PROGRAM(
				[[#include <opentelemetry/exporters/otlp/otlp_file_exporter_factory.h>]],
				[[OPENTELEMETRY_NAMESPACE::exporter::otlp::OtlpFileExporterFactory::Create();]]
			)], [
				AC_MSG_RESULT([yes])
				AC_DEFINE([HAVE_OTEL_EXPORTER_OTLP_FILE], [1], [Have OpenTelemetry OTLP/File exporter.])
				OPENTELEMETRY_LIBS="${OPENTELEMETRY_LIBS} -lopentelemetry_exporter_otlp_file -lopentelemetry_exporter_otlp_file_metric -lopentelemetry_exporter_otlp_file_log"
			], [AC_MSG_RESULT([no])]
		)

		AC_MSG_CHECKING([for OTLP/gRPC exporter])
		LIBS="${_saved_libs} ${OPENTELEMETRY_LIBS} -lopentelemetry_exporter_otlp_grpc -lopentelemetry_exporter_otlp_grpc_metrics -lopentelemetry_exporter_otlp_grpc_log"
		AC_LINK_IFELSE([
			AC_LANG_PROGRAM(
				[[#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h>]],
				[[OPENTELEMETRY_NAMESPACE::exporter::otlp::OtlpGrpcExporterFactory::Create();]]
			)], [
				AC_MSG_RESULT([yes])
				AC_DEFINE([HAVE_OTEL_EXPORTER_OTLP_GRPC], [1], [Have OpenTelemetry OTLP/gRPC exporter.])
				OPENTELEMETRY_LIBS="${OPENTELEMETRY_LIBS} -lopentelemetry_exporter_otlp_grpc -lopentelemetry_exporter_otlp_grpc_metrics -lopentelemetry_exporter_otlp_grpc_log"
			], [AC_MSG_RESULT([no])]
		)

		AC_MSG_CHECKING([for OTLP/HTTP exporter])
		LIBS="${_saved_libs} ${OPENTELEMETRY_LIBS} -lopentelemetry_exporter_otlp_http -lopentelemetry_exporter_otlp_http_metric -lopentelemetry_exporter_otlp_http_log"
		AC_LINK_IFELSE([
			AC_LANG_PROGRAM(
				[[#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>]],
				[[OPENTELEMETRY_NAMESPACE::exporter::otlp::OtlpHttpExporterFactory::Create();]]
			)], [
				AC_MSG_RESULT([yes])
				AC_DEFINE([HAVE_OTEL_EXPORTER_OTLP_HTTP], [1], [Have OpenTelemetry OTLP/HTTP exporter.])
				OPENTELEMETRY_LIBS="${OPENTELEMETRY_LIBS} -lopentelemetry_exporter_otlp_http -lopentelemetry_exporter_otlp_http_metric -lopentelemetry_exporter_otlp_http_log"
			], [AC_MSG_RESULT([no])]
		)

		AC_MSG_CHECKING([for Zipkin exporter])
		LIBS="${_saved_libs} ${OPENTELEMETRY_LIBS} -lopentelemetry_exporter_zipkin_trace"
		AC_LINK_IFELSE([
			AC_LANG_PROGRAM(
				[[#include <opentelemetry/exporters/zipkin/zipkin_exporter_factory.h>]],
				[[OPENTELEMETRY_NAMESPACE::exporter::zipkin::ZipkinExporterFactory::Create();]]
			)], [
				AC_MSG_RESULT([yes])
				AC_DEFINE([HAVE_OTEL_EXPORTER_ZIPKIN], [1], [Have OpenTelemetry Zipkin exporter.])
				OPENTELEMETRY_LIBS="${OPENTELEMETRY_LIBS} -lopentelemetry_exporter_zipkin_trace"
			], [AC_MSG_RESULT([no])]
		)
		AC_LANG_POP([C++])

		HAVE_OPENTELEMETRY=yes

		AX_VARIABLES_RESTORE

		AC_MSG_NOTICE([OpenTelemetry environment variables:])
		AC_MSG_NOTICE([  OPENTELEMETRY_CFLAGS=${OPENTELEMETRY_CFLAGS}])
		AC_MSG_NOTICE([  OPENTELEMETRY_CXXFLAGS=${OPENTELEMETRY_CXXFLAGS}])
		AC_MSG_NOTICE([  OPENTELEMETRY_CPPFLAGS=${OPENTELEMETRY_CPPFLAGS}])
		AC_MSG_NOTICE([  OPENTELEMETRY_LDFLAGS=${OPENTELEMETRY_LDFLAGS}])
		AC_MSG_NOTICE([  OPENTELEMETRY_LIBS=${OPENTELEMETRY_LIBS}])
		AC_MSG_NOTICE([  OPENTELEMETRY_INCLUDEDIR=${OPENTELEMETRY_INCLUDEDIR}])

		AC_SUBST([OPENTELEMETRY_CFLAGS])
		AC_SUBST([OPENTELEMETRY_CXXFLAGS])
		AC_SUBST([OPENTELEMETRY_CPPFLAGS])
		AC_SUBST([OPENTELEMETRY_LDFLAGS])
		AC_SUBST([OPENTELEMETRY_LIBS])
		AC_SUBST([OPENTELEMETRY_INCLUDEDIR])
	], [echo "OpenTelemetry not specified"])
])
