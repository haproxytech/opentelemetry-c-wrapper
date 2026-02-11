#!/bin/sh -ux
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_PKG_URL="https://github.com/aws/aws-lc/archive/refs/tags/AWS-LC-FIPS-3.0.0.tar.gz"

. "$(realpath "$(dirname "${0}")")/common.sh"


sh_configure_cmake \
	-DBUILD_TESTING=OFF \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DDISABLE_GO=ON
sh_make
