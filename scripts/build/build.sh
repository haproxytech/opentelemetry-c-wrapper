#!/bin/sh -u
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
# Build the complete dependency stack by running every individual library
# install script in the required order.  The installation prefix and staging
# directory can be overridden via command-line arguments.
#
export  SH_ARG_PREFIX="${1:-/opt}"
export SH_ARG_INSTDIR="${2:-/}"

SH_DIR="$(realpath "$(dirname "${0}")")"

. "${SH_DIR}/common.sh"


# libfyaml is no longer needed because opentelemetry-cpp already provides ryml.
#
"${SH_DIR}/aws-lc-AWS-LC-FIPS-3.0.0-install.sh"
"${SH_DIR}/curl-curl-8_19_0-install.sh"
"${SH_DIR}/abseil-cpp-20250127.1-install.sh"
"${SH_DIR}/c-ares-1.34.5-install.sh"
"${SH_DIR}/re2-2025-08-05-install.sh"
"${SH_DIR}/protobuf-29.5-install.sh"
"${SH_DIR}/json-3.12.0-install.sh"
"${SH_DIR}/googletest-1.17.0-install.sh"
"${SH_DIR}/benchmark-1.9.4-install.sh"
"${SH_DIR}/grpc-1.70.2-install.sh"
#"${SH_DIR}/rapidyaml-0.10.0-src-install.sh"
"${SH_DIR}/opentelemetry-cpp-1.26.0-install.sh"
#"${SH_DIR}/libfyaml-0.9-install.sh"

sh_ldd_check
date "+--- %F %T ----------"
