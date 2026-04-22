#!/bin/sh -u
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
# Build a minimal dependency bundle containing only the core libraries required
# by the project: AWS-LC, curl, the OpenTelemetry C++ SDK, and libfyaml.
# Arguments are the same as build.sh.
#
export  SH_ARG_PREFIX="${1:-/opt}"
export SH_ARG_INSTDIR="${2:-/}"
export SH_ARG_LIB_TYPE="${3:-dynamic}"

SH_DIR="$(realpath "$(dirname "${0}")")"

. "${SH_DIR}/common.sh"


# libfyaml is no longer needed because opentelemetry-cpp already provides ryml.
#
#"${SH_DIR}/aws-lc-AWS-LC-FIPS-3.0.0-install.sh"
#"${SH_DIR}/curl-curl-8_19_0-install.sh"
"${SH_DIR}/opentelemetry-cpp-1.26.0-install.sh"
#"${SH_DIR}/libfyaml-0.9-install.sh"

sh_ldd_check
date "+--- %F %T ----------"
