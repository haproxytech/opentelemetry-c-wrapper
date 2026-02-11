#!/bin/sh -ux
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_PKG_URL="https://github.com/curl/curl/archive/refs/tags/curl-8_19_0.tar.gz"

. "$(realpath "$(dirname "${0}")")/common.sh"


# LDAP is prohibited because the system library depends on the system's OpenSSL
# implementation.
#
sh_configure_cmake \
	-DOPENSSL_ROOT_DIR="${SH_ARG_PREFIX}" \
	-DCURL_DISABLE_LDAP=ON
sh_make
