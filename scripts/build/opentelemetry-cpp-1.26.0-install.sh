#!/bin/sh -ux
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_PKG_URL="https://github.com/open-telemetry/opentelemetry-cpp/archive/refs/tags/v1.26.0.tar.gz"

. "$(realpath "$(dirname "${0}")")/common.sh"


test "${SH_ARG_LIB_TYPE}" = "dynamic" && SH_SHARED_LIBS="ON"
test "${SH_ARG_LIB_TYPE}" = "static"  && SH_SHARED_LIBS="OFF"

# CMAKE_POLICY_VERSION_MINIMUM=3.5 is an added option in case cmake version 4.1
# (the latest) is used.
#
sh_configure_cmake \
	-DBUILD_PACKAGE=ON \
	-DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	-DCMAKE_CXX_STANDARD=14 \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DOPENSSL_ROOT_DIR="${SH_ARG_PREFIX}" \
	-DCURL_LIBRARY="${SH_SYS_LIBDIR}/libcurl.so" \
	-DZLIB_LIBRARY="${SH_SYS_LIBDIR}/libz.so" \
	-DZLIB_INCLUDE_DIR="/usr/include" \
	-DWITH_ABI_VERSION_1=OFF \
	-DWITH_ABI_VERSION_2=ON \
	-DWITH_CONFIGURATION=ON \
	-DOPENTELEMETRY_INSTALL=ON \
	-DWITH_NO_DEPRECATED_CODE=ON \
	-DWITH_OTLP_GRPC=ON \
	-DWITH_OTLP_HTTP=ON \
	-DWITH_OTLP_FILE=ON \
	-DWITH_OTLP_HTTP_COMPRESSION=ON \
	-DWITH_ZIPKIN=ON \
	-DWITH_ELASTICSEARCH=ON \
	-DOTELCPP_VERSIONED_LIBS=${SH_SHARED_LIBS} \
	-DWITH_ASYNC_EXPORT_PREVIEW=ON \
	-DWITH_THREAD_INSTRUMENTATION_PREVIEW=ON \
	-DWITH_METRICS_EXEMPLAR_PREVIEW=ON \
	-DWITH_OPENTRACING=OFF \
	-DWITH_BENCHMARK=OFF \
	-DWITH_EXAMPLES=OFF \
	-DWITH_FUNC_TESTS=OFF \
	-DBUILD_TESTING=OFF \
	-DBUILD_SHARED_LIBS=${SH_SHARED_LIBS} \
	-DCMAKE_BUILD_TYPE=Release
sh_make

# rapidyaml and c4core have errors in the cmake installation part and install
# the library in the lib directory instead of in lib64 (on linux systems that
# use the lib64 directory as the destination for the libraries).  That's why
# these libraries are explicitly transferred to the right destination.
#
# If the variable SH_LIBDIR_EXT is not set (that is where the destination
# directory for libraries is lib and not lib64), then nothing is done here.
#
if test -n "${SH_LIBDIR_EXT}"; then
	for _var_file in "${SH_LIBDIR}/cmake/"*/*; do
		sed -i "s#/lib/#/lib${SH_LIBDIR_EXT}/#g" "${_var_file}"
	done

	mv "${SH_LIBDIR}/cmake/"*     "${SH_LIBDIR}${SH_LIBDIR_EXT}/cmake"
	mv "${SH_LIBDIR}/pkgconfig/"* "${SH_LIBDIR}${SH_LIBDIR_EXT}/pkgconfig"
	mv "${SH_LIBDIR}/"*.so*       "${SH_LIBDIR}${SH_LIBDIR_EXT}"
	mv "${SH_LIBDIR}/"*.a         "${SH_LIBDIR}${SH_LIBDIR_EXT}"
	rmdir -p "${SH_LIBDIR}/*"
fi
