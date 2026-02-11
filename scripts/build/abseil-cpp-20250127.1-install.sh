#!/bin/sh -ux
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_PKG_URL="https://github.com/abseil/abseil-cpp/archive/refs/tags/20250127.1.tar.gz"

. "$(realpath "$(dirname "${0}")")/common.sh"


sh_configure_cmake \
	-DABSL_ENABLE_INSTALL=ON \
	-DABSL_PROPAGATE_CXX_STD=ON
sh_make
