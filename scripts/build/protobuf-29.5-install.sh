#!/bin/sh -ux
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_PKG_URL="https://github.com/protocolbuffers/protobuf/archive/refs/tags/v29.5.tar.gz"

. "$(realpath "$(dirname "${0}")")/common.sh"


sh_configure_cmake \
	-Dprotobuf_BUILD_TESTS=OFF \
	-Dprotobuf_ABSL_PROVIDER=package \
	-DZLIB_LIBRARY="${SH_SYS_LIBDIR}/libz.so"
sh_make
