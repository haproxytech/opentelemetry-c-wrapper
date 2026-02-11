#!/bin/sh -ux
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_PKG_URL="https://github.com/biojppm/rapidyaml/releases/download/v0.10.0/rapidyaml-0.10.0-src.tgz"

. "$(realpath "$(dirname "${0}")")/common.sh"


sh_configure_cmake \
	-DRYML_BUILD_TOOLS=ON
sh_make
