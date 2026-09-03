#!/bin/sh -ux
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_PKG_URL="https://github.com/madler/zlib/archive/refs/tags/v1.3.2.tar.gz"
SH_PKG_DEP="zlib"

. "$(realpath "$(dirname "${0}")")/common.sh"


# Only the selected library type is built, so that a shared object never sits
# next to the archive where the linker would prefer it.
#
sh_configure_cmake \
	-DZLIB_BUILD_SHARED=${SH_SHARED_LIBS} \
	-DZLIB_BUILD_STATIC=${SH_STATIC_LIBS} \
	-DZLIB_BUILD_TESTING=OFF
sh_make
