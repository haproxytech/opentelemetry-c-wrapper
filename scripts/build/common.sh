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
       SH_PKG_DEP="${SH_PKG_DEP:-}"
    SH_OPT_BUNDLE="${SH_OPT_BUNDLE:-}"
  SH_ARG_LIB_TYPE="${SH_ARG_LIB_TYPE:-dynamic}"
   SH_SHARED_LIBS=
   SH_STATIC_LIBS=
    SH_SYS_LIBDIR="/usr/lib/x86_64-linux-gnu"
        SH_LIBDIR="${SH_ARG_PREFIX}/lib"
    SH_LIBDIR_EXT=
      SH_EX_USAGE=64
    SH_EX_NOINPUT=66
   SH_EX_SOFTWARE=70

. /etc/os-release


# The lib-type decides whether every library built here comes out as a
# shared object or as a static archive.
#
if test "${SH_ARG_LIB_TYPE}" = "dynamic"; then
	SH_SHARED_LIBS="ON"
	SH_STATIC_LIBS="OFF"
elif test "${SH_ARG_LIB_TYPE}" = "static"; then
	SH_SHARED_LIBS="OFF"
	SH_STATIC_LIBS="ON"
else
	echo "ERROR: unknown lib-type '${SH_ARG_LIB_TYPE}', expected 'dynamic' or 'static'" >&2
	exit ${SH_EX_USAGE}
fi


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

# The install prefix is searched first, so that the libraries already built
# into it win over the system packages.  The archives are compiled as PIC so
# that they can still be linked into a shared object later on.  The policy
# floor lets cmake 4 configure the packages that still declare a minimum
# version below 3.5.
#
sh_configure_cmake ()
{
	cmake -DCMAKE_INSTALL_PREFIX="${SH_ARG_PREFIX}" \
		-DCMAKE_PREFIX_PATH="${SH_ARG_PREFIX}" \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
		-DCMAKE_C_FLAGS="-O2" \
		-DCMAKE_CXX_FLAGS="-O2" \
		${SH_CMAKE_ARGS} \
		-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON \
		-DCMAKE_INSTALL_LIBDIR=lib${SH_LIBDIR_EXT} \
		-DCMAKE_INSTALL_RPATH="${SH_LIBDIR}${SH_LIBDIR_EXT}" \
		-DCMAKE_POSITION_INDEPENDENT_CODE=ON \
		-DBUILD_SHARED_LIBS=${SH_SHARED_LIBS} \
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

	make -j${_var_cpu} all || return 1
	make DESTDIR="${SH_ARG_INSTDIR}" -j${_var_cpu} install
}

sh_archive ()
{
	# The tree assembled by opentelemetry-cpp-monorepo.sh carries the name
	# of the SDK install script, which pins the SDK version in one place.
	#
	local _var_cwd="${PWD}"
	local _var_src="${SH_PKG}"
	local _var_monorepo_dir="$(basename "$(dirname "${0}")"/opentelemetry-cpp-*-install.sh -install.sh)"

	#
	# A tree assembled by the opentelemetry-cpp-monorepo.sh script is used
	# as is: the release tarball is neither downloaded nor extracted over
	# it, so building from the assembled tree never touches the network.
	# A dependency whose sources that script staged under build/_deps of
	# the assembled tree is built from there for the same reason.
	#
	if test -n "${SH_PKG_DEP}" -a -f "${_var_monorepo_dir}/.monorepo" -a -d "${_var_monorepo_dir}/build/_deps/${SH_PKG_DEP}-src"; then
		_var_src="${_var_monorepo_dir}/build/_deps/${SH_PKG_DEP}-src"
	elif test ! -f "${SH_PKG}/.monorepo"; then
		test -f "${SH_PKG}.tar.gz" || wget "${SH_PKG_URL}" -O "${SH_PKG}.tar.gz"
		tar xf "${SH_PKG}.tar.gz"
	fi
	cd "${_var_src}" || exit ${SH_EX_NOINPUT}

	if test -f ".monorepo"; then
		#
		# A source tree carrying the .monorepo marker, assembled by the
		# opentelemetry-cpp-monorepo.sh script, is built as is: cmake uses
		# the bundled dependency sources unchanged instead of cloning them
		# again, and the dependencies bundled in the tree are never taken
		# from an installed package, so that a stale installation cannot
		# hijack the build.  The sources are first restored to the pristine
		# tag state so that a repeated run always patches and builds the
		# same tree.
		#
		SH_CMAKE_ARGS="${SH_CMAKE_ARGS} -DFETCHCONTENT_FULLY_DISCONNECTED=ON"
		SH_CMAKE_ARGS="${SH_CMAKE_ARGS} -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF"
		SH_CMAKE_ARGS="${SH_CMAKE_ARGS} -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF"
		SH_CMAKE_ARGS="${SH_CMAKE_ARGS} -DCMAKE_DISABLE_FIND_PACKAGE_ryml=ON"
		SH_CMAKE_ARGS="${SH_CMAKE_ARGS} -DCMAKE_DISABLE_FIND_PACKAGE_gRPC=ON"
		SH_CMAKE_ARGS="${SH_CMAKE_ARGS} -DCMAKE_DISABLE_FIND_PACKAGE_Protobuf=ON"
		SH_CMAKE_ARGS="${SH_CMAKE_ARGS} -DCMAKE_DISABLE_FIND_PACKAGE_absl=ON"

		git checkout -- . || exit 1
		find "${_var_cwd}" -maxdepth 1 -name "*${SH_PKG}.patch" -type f | grep -q . && {
			local _var_file=

			for _var_file in "${_var_cwd}/"*"${SH_PKG}.patch"; do
				git apply "${_var_file}" || exit 1
			done
		}
	else
		find "${_var_cwd}" -maxdepth 1 -name "*${SH_PKG}.patch" -type f | grep -q . && {
			local _var_file=

			for _var_file in "${_var_cwd}/"*"${SH_PKG}.patch"; do
				patch -p1 < "${_var_file}"
			done
		}
	fi

	mkdir -p build && cd build || exit 1
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
