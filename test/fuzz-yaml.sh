#!/bin/sh -u
#
# fuzz-yaml.sh by Miroslav Zagorac <mzagorac@haproxy.com>
#
# Build and run the libFuzzer harness for the YAML configuration loader.
# The instrumented library comes from fuzz-build.sh, which snapshots the
# source tree and builds it with clang, so the fuzzer gets coverage feedback
# from the library code and the source tree stays intact.  The first argument
# limits the fuzzing time in seconds, all following arguments are passed to
# the libFuzzer binary unchanged.
#
#   Usage: ./fuzz-yaml.sh [ time-limit [ libfuzzer-args ... ] ]
#
   SH_SRCDIR="$(realpath "$(dirname "${0}")/..")"
   SH_LIBDIR="/opt"
 SH_BUILDDIR="${TMPDIR:-/tmp}/otelc-fuzz"
   SH_SUFFIX="${OTELC_FUZZ_LIBFYAML:+-libfyaml}"
  SH_TREEDIR="${SH_BUILDDIR}/tree${SH_SUFFIX}"
   SH_CORPUS="${SH_BUILDDIR}/corpus"
    SH_TARGET="${SH_BUILDDIR}/fuzz-yaml${SH_SUFFIX}"
    SH_CLANG="${CLANG:-clang++}"
SH_ARG_TIME="${1:-60}"

test ${#} -gt 0 && shift

sh "${SH_SRCDIR}/test/fuzz-build.sh" || exit

"${SH_CLANG}" -O1 -g -fsanitize=fuzzer,address \
	-I "${SH_TREEDIR}/include" \
	-o "${SH_TARGET}" "${SH_SRCDIR}/test/fuzz-yaml.cpp" \
	-L "${SH_TREEDIR}/src/.libs" -lopentelemetry-c-wrapper || exit 70

if test ! -d "${SH_CORPUS}"; then
	mkdir -p "${SH_CORPUS}" || exit 73
	cp "${SH_SRCDIR}/test/otel-cfg.yml" "${SH_CORPUS}/seed-otel-cfg.yml"
	echo "contexts:" > "${SH_CORPUS}/seed-minimal.yml"
	echo "  default:" >> "${SH_CORPUS}/seed-minimal.yml"
	echo "handle_map_shards: 256" > "${SH_CORPUS}/seed-shards.yml"
fi

LD_LIBRARY_PATH="${SH_TREEDIR}/src/.libs:${SH_LIBDIR}/lib" \
	exec "${SH_TARGET}" \
		"${SH_CORPUS}" \
		-max_total_time="${SH_ARG_TIME}" \
		-timeout=25 \
		-close_fd_mask=1 \
		-artifact_prefix="${SH_BUILDDIR}/" "${@}"
