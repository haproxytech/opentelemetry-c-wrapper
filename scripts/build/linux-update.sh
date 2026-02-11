#!/bin/sh -u
#
# Copyright 2026 HAProxy Technologies, Miroslav Zagorac <mzagorac@haproxy.com>
#
# Interactively update the Linux system and install the development packages
# required to build the project and its dependencies.  Optionally installs a
# recent cmake from the Kitware APT repository on Ubuntu.
#
# The script has been tested on the following linux distributions:
#   - debian 11 (bullseye)
#     - install cmake from the bullseye-backports repository
#   - debian 12 (bookworm)
#   - debian 13 (trixie)
#   - rhel 8.10 (Ootpa)
#     - googletest-1.17.0 - cc1plus: error: -Werror=deprecated-copy: no option -Wdeprecated-copy
#     - install gcc-10 toolchain
#   - rhel 9.5 (Plow)
#   - rhel 10.0 (Coughlan)
#   - ubuntu 20.04.6 LTS (Focal Fossa)
#     - install cmake from the Kitware repository
#     - use CMAKE_POLICY_VERSION_MINIMUM=3.5 for cmake >= 4.1
#   - ubuntu 22.04.5 LTS (Jammy Jellyfish)
#   - ubuntu 24.04.3 LTS (Noble Numbat)
#   - tuxedo 24.04.3 LTS
#   - opensuse-leap 15.5
#     - install gcc10
#   - opensuse-leap 15.6
#     - install gcc10
#   - rocky 9.5 (Blue Onyx)
#
. /etc/os-release


sh_input ()
{
	local _arg_msg="${1}"
	local _var_input=

	printf '%s' "${_arg_msg}? [y/N] "
	read _var_input
	echo

	test "${_var_input}" = "y" -o "${_var_input}" = "Y"
}

sh_cmake_install ()
{
	local _var_keyfile="/usr/share/keyrings/kitware-archive-keyring.gpg"
	local _var_repolist="/etc/apt/sources.list.d/kitware.list"
	local _var_copyright="/usr/share/doc/kitware-archive-keyring/copyright"

	test "${ID}" = "ubuntu" || {
		echo "Unfortunately, there is no Kitware package repository for the used linux distribution (${ID} ${VERSION})"
		return 70
	}

	sh_input "Do you want to install the latest version of cmake utility, using Kitware repository" || return 0

	test -f "${_var_copyright}" || \
		wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - > "${_var_keyfile}"

	case "${ID}" in
	  ubuntu)
		echo "deb [signed-by=${_var_keyfile}] https://apt.kitware.com/${ID}/ ${VERSION_CODENAME} main" > "${_var_repolist}"
		apt update
		test -f "${_var_copyright}" || rm "${_var_keyfile}"
		apt install kitware-archive-keyring
		apt update
		apt install -y cmake
		;;
	esac
}

sh_system_update ()
{
	local _var_pkg_common="automake binutils cmake gawk gcc git libtool make pkgconf wget"
	local _var_pkg_debian="debhelper g++ libc6-dev libcurl4-gnutls-dev libgpg-error-dev libkeyutils-dev liblzma-dev libssl-dev libsystemd-dev zlib1g-dev"
	local _var_pkg_rhel="gcc-c++ glibc-devel libcurl-devel libgpg-error-devel patch rpm-build systemd-devel zlib-devel"

	sh_input "Do you want to update the linux system and install all necessary packages" || return 0

	case "${ID}" in
	  debian)
		export DEBIAN_FRONTEND="noninteractive"

		grep -q "^deb cdrom" /etc/apt/sources.list && \
			sh_input "Disable CD/DVD for package installation" && \
				sed -i "s/^\(deb cdrom.*\)/#\1/g" /etc/apt/sources.list
		apt-get update
		apt-get upgrade -y

		apt-get install -y ${_var_pkg_common} ${_var_pkg_debian}
		;;

	  opensuse-leap | sles)
		zypper update -y

		zypper install -y ${_var_pkg_common} ${_var_pkg_rhel} keyutils-devel krb5-devel libopenssl-devel libpsl-devel libzstd-devel lzma-sdk-devel
		;;

	  rhel | almalinux | centos | ol | rocky)
		yum upgrade -y

		yum install -y ${_var_pkg_common} ${_var_pkg_rhel} keyutils-libs-devel openssl-devel xz-devel
		;;

	  ubuntu | tuxedo)
		export DEBIAN_FRONTEND="noninteractive"

		apt update
		apt upgrade -y

		apt install -y ${_var_pkg_common} ${_var_pkg_debian}
		;;
	esac

	case "${ID}-${VERSION_ID}" in
	  debian-11)
		sed -i 's@\(deb\)\(\.debian\.org.*bullseye-backports\)@archive\2@' /etc/apt/sources.list
		apt-get update
		apt-get install -t bullseye-backports -y cmake
		;;

	  opensuse-leap-15.5 | sles-15.5)
		zypper addrepo "https://download.opensuse.org/repositories/devel:gcc/SLE-15/devel:gcc.repo"
		zypper refresh
		zypper install -y gcc10 gcc10-c++
		;;

	  opensuse-leap-15.6 | sles-15.6)
		zypper addrepo "https://download.opensuse.org/repositories/devel:gcc/openSUSE_Leap_${VERSION_ID}/devel:gcc.repo"
		zypper refresh
		zypper install -y gcc10 gcc10-c++
		;;

	  rhel-8.* | almalinux-8.* | rocky-8.*)
		yum install gcc-toolset-10.x86_64
		;;
	esac
}


date "+--- %F %T ----------"

sh_system_update
sh_cmake_install
