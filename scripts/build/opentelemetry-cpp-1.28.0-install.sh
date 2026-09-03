#!/bin/sh -ux
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_PKG_URL="https://github.com/open-telemetry/opentelemetry-cpp/archive/refs/tags/v1.28.0.tar.gz"

. "$(realpath "$(dirname "${0}")")/common.sh"


# find_package(ryml) is disabled because the SDK does not check the ryml
# version, so a stale ryml in the prefix would silently override the pinned
# one; the pinned version is always fetched and built instead.
#
# AWS-LC, curl and zlib are taken as installed packages: a static build finds
# the archives that build-bundle.sh installed into the prefix beforehand, a
# dynamic build finds the shared objects there, or falls back to the ones of
# the system.
#
# A bundle build leaves abseil, protobuf, gRPC and nlohmann json to the SDK,
# which compiles the pinned versions itself and installs them into the prefix,
# so that an installation of them found elsewhere never takes their place.
#
if test "${SH_OPT_BUNDLE}" = "true"; then
	SH_CMAKE_ARGS="${SH_CMAKE_ARGS} -DCMAKE_DISABLE_FIND_PACKAGE_gRPC=ON"
	SH_CMAKE_ARGS="${SH_CMAKE_ARGS} -DCMAKE_DISABLE_FIND_PACKAGE_Protobuf=ON"
	SH_CMAKE_ARGS="${SH_CMAKE_ARGS} -DCMAKE_DISABLE_FIND_PACKAGE_absl=ON"
	SH_CMAKE_ARGS="${SH_CMAKE_ARGS} -DCMAKE_DISABLE_FIND_PACKAGE_nlohmann_json=ON"
fi

sh_configure_cmake \
	-DBUILD_PACKAGE=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_ryml=ON \
	-DCMAKE_CXX_STANDARD=17 \
	-DOPENSSL_ROOT_DIR="${SH_ARG_PREFIX}" \
	-DOPENSSL_USE_STATIC_LIBS=${SH_STATIC_LIBS} \
	-DCURL_USE_STATIC_LIBS=${SH_STATIC_LIBS} \
	-DZLIB_USE_STATIC_LIBS=${SH_STATIC_LIBS} \
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
	-DBUILD_TESTING=OFF
sh_make || exit ${SH_EX_SOFTWARE}

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
	rmdir -p "${SH_LIBDIR}/"*
fi
