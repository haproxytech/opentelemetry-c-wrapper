#!/bin/sh -ux
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_PKG_URL="https://github.com/pantoniou/libfyaml/archive/refs/tags/v0.9.tar.gz"

. "$(realpath "$(dirname "${0}")")/common.sh"


cd ..
./bootstrap.sh
cd -

sh_configure \
	--enable-silent-rules \
	--enable-portable-target
sh_make
