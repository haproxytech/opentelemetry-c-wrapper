#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
# Shared variables and helper functions sourced by each individual library
# install script.  Provides cmake and configure wrappers, parallel make, archive
# extraction, shared-library validation, and per-distribution compiler and path
# adjustments.
#
           SH_PKG="$(basename "${0}" -install.sh)"
    SH_CMAKE_ARGS=
SH_CONFIGURE_ARGS=
       SH_PKG_URL="${SH_PKG_URL:-}"
    SH_SYS_LIBDIR="/usr/lib/x86_64-linux-gnu"
        SH_LIBDIR="${SH_ARG_PREFIX}/lib"
    SH_LIBDIR_EXT=

. /etc/os-release


sh_ldd_check ()
{
	local _var_file=
	local _var_retval=0

	for _var_file in "${SH_LIBDIR}${SH_LIBDIR_EXT}/"*.so "${SH_ARG_PREFIX}/bin/"*; do
		ldd "${_var_file}" 2>/dev/null | grep -q "not found" && {
			echo "${_var_file}"
			objdump -x "${_var_file}" | grep PATH
			ldd "${_var_file}" | grep "not found"

			_var_retval=1
		}
	done

	echo
	test ${_var_retval} -eq 0 && echo "ldd - everything went OK!" || echo "ldd - some errors occurred"

	return ${_var_retval}
}

sh_configure_cmake ()
{
	cmake -DCMAKE_INSTALL_PREFIX="${SH_ARG_PREFIX}" \
		-DCMAKE_C_FLAGS="-O2" \
		-DCMAKE_CXX_FLAGS="-O2" \
		${SH_CMAKE_ARGS} \
		-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON \
		-DCMAKE_INSTALL_LIBDIR=lib${SH_LIBDIR_EXT} \
		-DCMAKE_INSTALL_RPATH="${SH_LIBDIR}${SH_LIBDIR_EXT}" \
		-DBUILD_SHARED_LIBS=ON \
		-DCMAKE_BUILD_TYPE=Release \
		"${@}" ..
}

sh_configure ()
{
	CFLAGS="-O2" CXXFLAGS="-O2" ../configure --prefix="${SH_ARG_PREFIX}" \
		${SH_CONFIGURE_ARGS} \
		--libdir="\${exec_prefix}/lib${SH_LIBDIR_EXT}" \
		"${@}"
}

sh_make ()
{
	local _var_cpu="$(grep "^processor" /proc/cpuinfo | wc -l)"

	make -j${_var_cpu} all
	make DESTDIR="${SH_ARG_INSTDIR}" -j${_var_cpu} install
}

sh_archive ()
{
	test -f "${SH_PKG}.tar.gz" || wget "${SH_PKG_URL}" -O "${SH_PKG}.tar.gz"
	tar xf "${SH_PKG}.tar.gz"
	cd "${SH_PKG}"
	find .. -maxdepth 1 -name "*${SH_PKG}.patch" -type f | grep -q . && patch -p1 < "../*${SH_PKG}.patch"

	mkdir build && cd build || exit 1
}


case "${ID}" in
  opensuse-leap | sles)
	SH_SYS_LIBDIR="/usr/lib64"
	SH_LIBDIR_EXT="64"
	test -e /usr/bin/gcc-10 && \
		SH_CMAKE_ARGS="${SH_CMAKE_ARGS} -DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10"
	;;

  rhel | almalinux | centos | ol | rocky)
	SH_SYS_LIBDIR="/usr/lib64"
	SH_LIBDIR_EXT="64"
	test -e /opt/rh/gcc-toolset-10/root/bin/gcc && \
		SH_CMAKE_ARGS="${SH_CMAKE_ARGS} -DCMAKE_C_COMPILER=/opt/rh/gcc-toolset-10/root/bin/gcc -DCMAKE_CXX_COMPILER=/opt/rh/gcc-toolset-10/root/bin/g++"
	;;
esac

if test -z "${SH_PKG_URL}"; then
	date "+--- %F %T ----------"
else
	sh_archive
fi
