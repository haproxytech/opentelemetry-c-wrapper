#!/bin/sh -u
#
# fuzz-extract.sh by Miroslav Zagorac <mzagorac@haproxy.com>
#
# Build and run the libFuzzer harness for the carrier extraction paths.
# The library is built with clang into a separate out-of-tree directory,
# instrumented with -fsanitize=address,fuzzer-no-link so the fuzzer gets
# coverage feedback from the library code; the source tree stays intact.
# The first argument limits the fuzzing time in seconds, all following
# arguments are passed to the libFuzzer binary unchanged.
#
#   Usage: ./fuzz-extract.sh [ time-limit [ libfuzzer-args ... ] ]
#
   SH_SRCDIR="$(realpath "$(dirname "${0}")/..")"
   SH_LIBDIR="/opt"
 SH_BUILDDIR="${TMPDIR:-/tmp}/otelc-fuzz"
   SH_CORPUS="${SH_BUILDDIR}/corpus-extract"
    SH_CLANG="${CLANG:-clang++}"
SH_ARG_TIME="${1:-60}"

test ${#} -gt 0 && shift

command -v "${SH_CLANG}" >/dev/null 2>&1 || { echo "ERROR: ${SH_CLANG} not found"; exit 69; }

test -x "${SH_SRCDIR}/configure" || (cd "${SH_SRCDIR}" && sh scripts/bootstrap) || exit 70

if test ! -e "${SH_BUILDDIR}/src/.libs/libopentelemetry-c-wrapper.so"; then
	if test -e "${SH_SRCDIR}/config.status"; then
		echo "ERROR: ${SH_SRCDIR} is configured in tree; run scripts/distclean there first"
		exit 72
	fi

	mkdir -p "${SH_BUILDDIR}" || exit 73
	cd "${SH_BUILDDIR}" || exit 71

	CC=clang \
	CXX="${SH_CLANG}" \
	CFLAGS="-O1 -g -fsanitize=address,fuzzer-no-link" \
	CXXFLAGS="-O1 -g -fsanitize=address,fuzzer-no-link" \
	LDFLAGS="-fsanitize=address" \
		"${SH_SRCDIR}/configure" --prefix="${SH_LIBDIR}" --with-opentelemetry="${SH_LIBDIR}" || exit 78

	make -j8 || exit 70
fi

"${SH_CLANG}" -O1 -g -fsanitize=fuzzer,address \
	-I "${SH_SRCDIR}/include" -I "${SH_BUILDDIR}/include" \
	-o "${SH_BUILDDIR}/fuzz-extract" "${SH_SRCDIR}/test/fuzz-extract.cpp" \
	-L "${SH_BUILDDIR}/src/.libs" -lopentelemetry-c-wrapper || exit 70

if test ! -d "${SH_CORPUS}"; then
	mkdir -p "${SH_CORPUS}" || exit 73
	echo "00-f067aa0ba902b7-01" > "${SH_CORPUS}/seed-traceparent.txt"
	echo "traceparent:00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01" > "${SH_CORPUS}/seed-w3c.txt"
	echo "tracestate:congo=t61rcWkgMzE,rojo=00f067aa0ba902b7" >> "${SH_CORPUS}/seed-w3c.txt"
	echo "baggage:userId=alice,serverNode=DF%2028,isProduction=false" > "${SH_CORPUS}/seed-baggage.txt"
	echo "b3:4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-1" > "${SH_CORPUS}/seed-b3.txt"
fi

OTELC_FUZZ_CFG="${SH_SRCDIR}/test/otel-cfg.yml" \
LD_LIBRARY_PATH="${SH_BUILDDIR}/src/.libs:${SH_LIBDIR}/lib" \
	exec "${SH_BUILDDIR}/fuzz-extract" \
		"${SH_CORPUS}" \
		-max_total_time="${SH_ARG_TIME}" \
		-close_fd_mask=1 \
		-artifact_prefix="${SH_BUILDDIR}/" "${@}"
