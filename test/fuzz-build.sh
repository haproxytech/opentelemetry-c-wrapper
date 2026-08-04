#!/bin/sh -u
#
# fuzz-build.sh by Miroslav Zagorac <mzagorac@haproxy.com>
#
# Build the clang instrumented copy of the library that the fuzzing harnesses
# link against.  The source tree is snapshotted into the fuzzing build
# directory and configured there, so a harness builds whether or not the source
# tree itself is configured, and the tree is never written to.  The build
# artifacts of the source tree are left out of the snapshot, so no stale object
# can survive into the instrumented library.  The snapshot is rebuilt from
# scratch whenever a library source or a build system input is newer than
# that library; the build artifacts of the tree are not consulted.
#
#   Usage: ./fuzz-build.sh
#
  SH_SRCDIR="$(realpath "$(dirname "${0}")/..")"
  SH_LIBDIR="/opt"
SH_BUILDDIR="${TMPDIR:-/tmp}/otelc-fuzz"
 SH_TREEDIR="${SH_BUILDDIR}/tree"
     SH_LIB="${SH_TREEDIR}/src/.libs/libopentelemetry-c-wrapper.so"
   SH_CLANG="${CLANG:-clang++}"

command -v "${SH_CLANG}" >/dev/null 2>&1 || { echo "ERROR: ${SH_CLANG} not found"; exit 69; }

if test -e "${SH_LIB}"; then
	test -z "$(find "${SH_SRCDIR}/src" "${SH_SRCDIR}/include" "${SH_SRCDIR}/m4" \
		"${SH_SRCDIR}/configure.ac" "${SH_SRCDIR}/Makefile.am" -type f \
		\( -name '*.cpp' -o -name '*.h' -o -name '*.h.in' -o -name '*.m4' \
		-o -name '*.ac' -o -name '*.am' \) -newer "${SH_LIB}" 2>/dev/null | head -n 1)" && exit 0

	echo "*** the library sources changed, rebuilding the instrumented copy ***"
fi

rm -rf "${SH_TREEDIR}"
mkdir -p "${SH_TREEDIR}" || exit 73
tar cf - -C "${SH_SRCDIR}" \
	--anchored --exclude=./configure --exclude=./config --exclude=./config.status \
	--exclude=./config.log --exclude=./aclocal.m4 --exclude=./libtool \
	--exclude=./stamp-h1 --exclude=./autom4te.cache \
	--no-anchored --exclude=.git --exclude=.libs --exclude=.deps \
	--exclude=Makefile --exclude=Makefile.in --exclude=core \
	--exclude='*.o' --exclude='*.lo' --exclude='*.la' --exclude='*.so*' \
	--exclude='*~' --exclude='_*' . | tar xf - -C "${SH_TREEDIR}" || exit 74

cd "${SH_TREEDIR}" || exit 71

sh scripts/bootstrap >/dev/null || exit 70

CC=clang \
CXX="${SH_CLANG}" \
CFLAGS="-O1 -g -fsanitize=address,fuzzer-no-link" \
CXXFLAGS="-O1 -g -fsanitize=address,fuzzer-no-link" \
LDFLAGS="-fsanitize=address" \
	./configure --prefix="${SH_LIBDIR}" --with-opentelemetry="${SH_LIBDIR}" || exit 78

make -j8 || exit 70
