#!/bin/sh -ux
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_PKG_URL="https://github.com/google/benchmark/archive/refs/tags/v1.9.4.tar.gz"

. "$(realpath "$(dirname "${0}")")/common.sh"


sh_configure_cmake \
	-DBENCHMARK_USE_BUNDLED_GTEST=OFF
sh_make
