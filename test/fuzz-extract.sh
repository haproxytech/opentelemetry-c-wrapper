#!/bin/sh -u
#
# fuzz-extract.sh by Miroslav Zagorac <mzagorac@haproxy.com>
#
# Build and run the libFuzzer harness for the carrier extraction paths.
# The instrumented library comes from fuzz-build.sh, which snapshots the
# source tree and builds it with clang, so the fuzzer gets coverage feedback
# from the library code and the source tree stays intact.  The first argument
# limits the fuzzing time in seconds, all following arguments are passed to
# the libFuzzer binary unchanged.
#
#   Usage: ./fuzz-extract.sh [ time-limit [ libfuzzer-args ... ] ]
#
   SH_SRCDIR="$(realpath "$(dirname "${0}")/..")"
   SH_LIBDIR="/opt"
 SH_BUILDDIR="${TMPDIR:-/tmp}/otelc-fuzz"
  SH_TREEDIR="${SH_BUILDDIR}/tree"
   SH_CORPUS="${SH_BUILDDIR}/corpus-extract"
    SH_CLANG="${CLANG:-clang++}"
SH_ARG_TIME="${1:-60}"

test ${#} -gt 0 && shift

sh "${SH_SRCDIR}/test/fuzz-build.sh" || exit

"${SH_CLANG}" -O1 -g -fsanitize=fuzzer,address \
	-I "${SH_TREEDIR}/include" \
	-o "${SH_BUILDDIR}/fuzz-extract" "${SH_SRCDIR}/test/fuzz-extract.cpp" \
	-L "${SH_TREEDIR}/src/.libs" -lopentelemetry-c-wrapper || exit 70

if test ! -d "${SH_CORPUS}"; then
	mkdir -p "${SH_CORPUS}" || exit 73
	echo "00-f067aa0ba902b7-01" > "${SH_CORPUS}/seed-traceparent.txt"
	echo "traceparent:00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01" > "${SH_CORPUS}/seed-w3c.txt"
	echo "tracestate:congo=t61rcWkgMzE,rojo=00f067aa0ba902b7" >> "${SH_CORPUS}/seed-w3c.txt"
	echo "baggage:userId=alice,serverNode=DF%2028,isProduction=false" > "${SH_CORPUS}/seed-baggage.txt"
	echo "b3:4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-1" > "${SH_CORPUS}/seed-b3.txt"
fi

OTELC_FUZZ_CFG="${SH_SRCDIR}/test/otel-cfg.yml" \
LD_LIBRARY_PATH="${SH_TREEDIR}/src/.libs:${SH_LIBDIR}/lib" \
	exec "${SH_BUILDDIR}/fuzz-extract" \
		"${SH_CORPUS}" \
		-max_total_time="${SH_ARG_TIME}" \
		-close_fd_mask=1 \
		-artifact_prefix="${SH_BUILDDIR}/" "${@}"
