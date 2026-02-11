#!/bin/sh -ux
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_PKG_URL="https://github.com/grpc/grpc/archive/refs/tags/v1.70.2.tar.gz"

. "$(realpath "$(dirname "${0}")")/common.sh"

case "${ID}" in
  opensuse-leap | sles | rhel | almalinux | centos | ol | rocky)
	SH_CMAKE_ARGS="${SH_CMAKE_ARGS} -DgRPC_INSTALL_LIBDIR=lib${SH_LIBDIR_EXT} -DgRPC_INSTALL_CMAKEDIR=lib${SH_LIBDIR_EXT}/cmake/grpc"
	;;
esac


sh_configure_cmake \
	-DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	-DgRPC_INSTALL=ON \
	-DgRPC_BUILD_TESTS=OFF \
	-DgRPC_ABSL_PROVIDER=package \
	-DgRPC_CARES_PROVIDER=package \
	-DgRPC_PROTOBUF_PROVIDER=package \
	-DOPENSSL_ROOT_DIR="${SH_ARG_PREFIX}" \
	-DgRPC_RE2_PROVIDER=package \
	-DgRPC_SSL_PROVIDER=package \
	-DgRPC_ZLIB_PROVIDER=package \
	-DZLIB_LIBRARY="${SH_SYS_LIBDIR}/libz.so" \
	-DgRPC_BUILD_GRPC_CSHARP_PLUGIN=OFF \
	-DgRPC_BUILD_GRPC_NODE_PLUGIN=OFF \
	-DgRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN=OFF \
	-DgRPC_BUILD_GRPC_PHP_PLUGIN=OFF \
	-DgRPC_BUILD_GRPC_PYTHON_PLUGIN=OFF \
	-DgRPC_BUILD_GRPC_RUBY_PLUGIN=OFF
sh_make
