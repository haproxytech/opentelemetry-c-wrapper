#!/bin/sh -ux
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_PKG_URL="https://github.com/nlohmann/json/archive/refs/tags/v3.12.0.tar.gz"

. "$(realpath "$(dirname "${0}")")/common.sh"


sh_configure_cmake \
	-DJSON_BuildTests=OFF \
	-DJSON_MultipleHeaders=OFF
sh_make
