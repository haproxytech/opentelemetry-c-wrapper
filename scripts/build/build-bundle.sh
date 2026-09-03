#!/bin/sh -u
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
# Build a minimal dependency bundle: the SSL library (AWS-LC, or OpenSSL in
# its place) and curl are built first, and the OpenTelemetry C++ SDK follows,
# with its own cmake configuration downloading and compiling the third-party
# libraries it needs.  curl is compiled against the bundled SSL library on
# purpose, so that the whole bundle shares one SSL library and the curl of
# the system, tied to the system's SSL library, stays out of it.  The
# libfyaml step is kept commented out.  Arguments are the same as build.sh,
# with an additional lib-type argument that selects either dynamic or static
# libraries.  A dynamic build takes zlib from the system, while a static build
# compiles it here as an archive: the SDK accepts zlib only as an installed
# package, and the system ships it as a shared object.
#
export  SH_ARG_PREFIX="${1:-/opt}"
export SH_ARG_INSTDIR="${2:-/}"
export SH_ARG_LIB_TYPE="${3:-dynamic}"
export  SH_OPT_BUNDLE="true"

SH_DIR="$(realpath "$(dirname "${0}")")"

. "${SH_DIR}/common.sh"


if test "${SH_ARG_LIB_TYPE}" = "static"; then
	"${SH_DIR}/zlib-1.3.2-install.sh" || exit ${SH_EX_SOFTWARE}
fi

# OpenSSL can take the place of AWS-LC: swap the comment marks of the two rows.
#
"${SH_DIR}/aws-lc-AWS-LC-FIPS-3.0.0-install.sh" || exit ${SH_EX_SOFTWARE}
#"${SH_DIR}/openssl-3.5.8-install.sh" || exit ${SH_EX_SOFTWARE}
"${SH_DIR}/curl-curl-8_21_0-install.sh" || exit ${SH_EX_SOFTWARE}
"${SH_DIR}/opentelemetry-cpp-1.28.0-install.sh" || exit ${SH_EX_SOFTWARE}

# libfyaml is no longer needed because opentelemetry-cpp already provides ryml.
#
#"${SH_DIR}/libfyaml-0.9-install.sh"

sh_ldd_check
date "+--- %F %T ----------"
