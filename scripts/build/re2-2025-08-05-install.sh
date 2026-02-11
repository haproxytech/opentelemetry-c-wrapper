#!/bin/sh -ux
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_PKG_URL="https://github.com/google/re2/archive/refs/tags/2025-08-05.tar.gz"

. "$(realpath "$(dirname "${0}")")/common.sh"


sh_configure_cmake
sh_make
