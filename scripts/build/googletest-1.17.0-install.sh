#!/bin/sh -ux
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_PKG_URL="https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz"

. "$(realpath "$(dirname "${0}")")/common.sh"


sh_configure_cmake \
	-DCMAKE_CXX_FLAGS="-Wno-error=deprecated-copy"
sh_make
