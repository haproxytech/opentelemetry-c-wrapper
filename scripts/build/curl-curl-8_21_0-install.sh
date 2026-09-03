#!/bin/sh -ux
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_PKG_URL="https://github.com/curl/curl/archive/refs/tags/curl-8_21_0.tar.gz"
SH_PKG_DEP="curl"

. "$(realpath "$(dirname "${0}")")/common.sh"


# LDAP is prohibited because the system library depends on the system's OpenSSL
# implementation.
#
# The optional backends (HTTP/2, IDN, PSL, SSH, brotli, zstd) are switched off:
# the OTLP exporters need none of them, and a static libcurl.a would otherwise
# force their libraries onto the link line of every consumer.
#
sh_configure_cmake \
	-DOPENSSL_ROOT_DIR="${SH_ARG_PREFIX}" \
	-DOPENSSL_USE_STATIC_LIBS=${SH_STATIC_LIBS} \
	-DZLIB_USE_STATIC_LIBS=${SH_STATIC_LIBS} \
	-DBUILD_STATIC_LIBS=${SH_STATIC_LIBS} \
	-DCURL_DISABLE_LDAP=ON \
	-DCURL_USE_LIBPSL=OFF \
	-DCURL_USE_LIBSSH2=OFF \
	-DUSE_LIBIDN2=OFF \
	-DUSE_NGHTTP2=OFF \
	-DCURL_BROTLI=OFF \
	-DCURL_ZSTD=OFF
sh_make
