#!/bin/sh -ux
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_PKG_URL="https://github.com/openssl/openssl/releases/download/openssl-3.5.8/openssl-3.5.8.tar.gz"

. "$(realpath "$(dirname "${0}")")/common.sh"


# OpenSSL comes with its own Configure script, so the cmake wrapper does not
# apply.  The library directory is named explicitly, as Configure would pick
# lib64 on its own, and the archives of a static build are compiled as PIC so
# that they can still be linked into a shared object.  A shared build records
# the library directory as its run path, as the cmake wrapper does for the other
# libraries, so that libssl resolves the libcrypto of the bundle.
#
if test "${SH_ARG_LIB_TYPE}" = "static"; then
	SH_CONFIGURE_ARGS="no-shared -fPIC"
else
	SH_CONFIGURE_ARGS="shared -Wl,-rpath,${SH_LIBDIR}${SH_LIBDIR_EXT}"
fi

../Configure --prefix="${SH_ARG_PREFIX}" --libdir="lib${SH_LIBDIR_EXT}" \
	${SH_CONFIGURE_ARGS} \
	no-docs \
	no-tests
sh_make
