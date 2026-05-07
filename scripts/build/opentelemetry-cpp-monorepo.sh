#!/bin/sh -u
#
# opentelemetry-cpp-monorepo.sh by Miroslav Zagorac <mzagorac@haproxy.com>
#
# Assembles a self-contained opentelemetry-cpp 1.26.0 source tree along with
# the curated set of pinned third-party C/C++ dependencies it expects to find
# at build time.  Each dependency is cloned from GitHub into the layout that
# the upstream build system expects, with a local main/master branch checked
# out at the requested tag and all nested submodules initialized recursively.
#
# By default the origin remote is dropped from every clone, producing a fully
# offline working copy that builds without any further network access -- ideal
# for archiving, transferring to an air-gapped host, or seeding an independent
# local repository.  Pass any non-empty value as the second argument to keep
# the upstream remotes attached.
#
   SH_ARG_DIR="${1:+${1}/}"
SH_OPT_REMOTE="${2:-}"
  SH_REPO_DIR="opentelemetry-cpp-1.26.0"
  SH_GIT_OPTS="-c advice.detachedHead=false clone --single-branch --depth 1 --branch"
       SH_PWD=
  SH_TAG_REPO="
	master v1.3.0      - jupp0r/prometheus-cpp              third_party/prometheus-cpp
	master 2024.02.14  - Microsoft/vcpkg                    tools/vcpkg
	main   v4.2.1      - microsoft/GSL                      third_party/ms-gsl
	main   v1.17.0     - google/googletest                  third_party/googletest
	main   v1.9.4      - google/benchmark                   third_party/benchmark
	main   v1.8.0      - open-telemetry/opentelemetry-proto third_party/opentelemetry-proto
	master v3.12.0     - nlohmann/json                      third_party/nlohmann-json
	master v1.6.0      - opentracing/opentracing-cpp        third_party/opentracing-cpp
	master 20250512.1  - abseil/abseil-cpp                  build/_deps/abesil-cpp-src
	master curl-8_19_0 - curl/curl                          build/_deps/curl-src
	main   v6.31.1     - protocolbuffers/protobuf           build/_deps/protobuf-src
	master v1.78.1     1 grpc/grpc                          build/_deps/grpc-src
	master v0.10.0     2 biojppm/rapidyaml                  build/_deps/ryml-src
	master v1.3.2      - madler/zlib                        build/_deps/zlib-src
"

sh_get()
{
	local _arg_repo="${1}"
	local _arg_branch="${2}"
	local _arg_tag="${3}"
	local _arg_dir="${4}"
	local _arg_sub="${5}"

	git ${SH_GIT_OPTS} "${_arg_tag}" "https://github.com/${_arg_repo}.git" "${SH_ARG_DIR}${_arg_dir}"
	cd "${SH_ARG_DIR}${_arg_dir}"
	git checkout -b "${_arg_branch}" "${_arg_tag}"
	test "${_arg_sub}" = "1" && git submodule update --init
	test "${_arg_sub}" = "2" && git submodule update --init --recursive
	test -n "${SH_OPT_REMOTE}" || git remote remove origin
	cd "${SH_PWD}"
}


git ${SH_GIT_OPTS} v1.26.0 https://github.com/open-telemetry/opentelemetry-cpp.git "${SH_ARG_DIR}${SH_REPO_DIR}"
cd "${SH_ARG_DIR}${SH_REPO_DIR}" || exit
git checkout -b main v1.26.0

SH_PWD="${PWD}"

printf '%s\n' "${SH_TAG_REPO}" | while read _var_branch _var_tag _var_sub _var_repo _var_dir; do
	test -n "${_var_branch}" && \
		sh_get "${_var_repo}" "${_var_branch}" "${_var_tag}" "${_var_dir}" "${_var_sub}"
done
