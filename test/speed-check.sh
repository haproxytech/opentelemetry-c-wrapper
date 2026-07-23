#!/bin/sh -u
#
# speed-check.sh by Miroslav Zagorac <mzagorac@haproxy.com>
#
# Compare the current wrapper throughput against a machine-local baseline
# and fail when a thread group regresses beyond the tolerance or when the
# scaling ratio between the last and the first group falls below the
# minimum.  Without a readable baseline (or with -R) the current rates
# are recorded as the new baseline instead.  See README-speed_check.
#
#   Usage: ./speed-check.sh [-b baseline] [-c test-cfg] [-g groups] [-h]
#                           [-i instances] [-R] [-r runtime_ms] [-s min_scale]
#                           [-T tolerance_pct] [-t test-program]
#
          SH_DIR="$(realpath "$(dirname "${0}")")"
         SH_DATE="$(date +%Y%m%d-%H%M%S)"
          SH_CPU="$(awk -F': ' '/^model name/ { print $2; exit }' /proc/cpuinfo)"
      SH_VERSION=
      SH_VARIANT="release"
       SH_RETVAL="0"
          SH_CTX="speed_test"
    SH_USAGE_MSG="usage: $(basename "${0}") [-b baseline] [-c test-cfg] [-g groups] [-h] [-i instances] [-R] [-r runtime_ms] [-s min_scale] [-T tolerance_pct] [-t test-program]"

     SH_ARG_TEST=
     SH_ARG_BASE="${SH_DIR}/_speed_baseline"
      SH_ARG_CFG="${SH_DIR}/otel-cfg.yml"
   SH_ARG_GROUPS="1 4 32 256"
SH_ARG_INSTANCES="1 2 4"
  SH_ARG_RUNTIME="10000"
    SH_ARG_SCALE="1.2"
      SH_ARG_TOL="15"
   SH_ARG_RECORD=


sh_measure()
{
	local _arg_instances="${1}"
	local _arg_threads="${2}"

	"${SH_ARG_TEST}" -D0 -c "${SH_ARG_CFG}" -n "${SH_CTX}" -i"${_arg_instances}" -r"${SH_ARG_RUNTIME}" -t"${_arg_threads}" -s1 2>/dev/null | \
		awk '/worker\(s\) total count:/ { r += $6 } END { printf "%.0f\n", r }'
}

sh_log()
{
	local _arg_file="${1}"
	local _arg_instances="${2}"
	local _arg_threads="${3}"
	local _arg_rate="${4}"

	test -e "${_arg_file}" || {
		echo "# cpu: ${SH_CPU}" > "${_arg_file}"
		echo "# version: ${SH_VERSION}" >> "${_arg_file}"
		echo "# otel-c-wrapper speed baseline: variant runtime_ms instances threads rate" >> "${_arg_file}"
	}

	echo "${SH_VARIANT} ${SH_ARG_RUNTIME} ${_arg_instances} ${_arg_threads} ${_arg_rate}" >> "${_arg_file}"
}


while getopts b:c:g:hi:Rr:s:T:t: c; do
	case "${c}" in
	  b)	SH_ARG_BASE="$(realpath "${OPTARG}")" || exit 64 ;;
	  c)	SH_ARG_CFG="$(realpath "${OPTARG}")" || exit 64 ;;
	  g)	SH_ARG_GROUPS="${OPTARG}" ;;
	  h)	echo "${SH_USAGE_MSG}"; exit 0 ;;
	  i)	SH_ARG_INSTANCES="${OPTARG}" ;;
	  R)	SH_ARG_RECORD="yes" ;;
	  r)	SH_ARG_RUNTIME="${OPTARG}" ;;
	  s)	SH_ARG_SCALE="${OPTARG}" ;;
	  T)	SH_ARG_TOL="${OPTARG}" ;;
	  t)	SH_ARG_TEST="$(realpath "${OPTARG}")" || exit 64 ;;
	  \?)	echo "${SH_USAGE_MSG}"; exit 64 ;;
	esac
done

shift $((OPTIND - 1))

if test -n "${SH_ARG_TEST}"; then
	case "${SH_ARG_TEST}" in
	  *_dbg)	SH_VARIANT="debug" ;;
	esac
elif test -x "${SH_DIR}/otel-c-wrapper-test"; then
	SH_ARG_TEST="${SH_DIR}/otel-c-wrapper-test"
else
	SH_ARG_TEST="${SH_DIR}/otel-c-wrapper-test_dbg"
	SH_VARIANT="debug"
fi

test -x "${SH_ARG_TEST}" || { echo "ERROR: test program missing, build the test target first"; exit 69; }

SH_VERSION="$("${SH_ARG_TEST}" -V 2>/dev/null | awk '$3 == "[build" { sub(/^v/, "", $2); sub(/]$/, "", $4); printf "%s-%s\n", $2, $4; exit }')"

if test -n "${SH_ARG_RECORD}" && test -e "${SH_ARG_BASE}"; then
	mv "${SH_ARG_BASE}" "${SH_ARG_BASE}-old-${SH_DATE}" || exit 73
fi

if test ! -r "${SH_ARG_BASE}"; then
	printf " %9s  %9s  %7s  %8s\n" "recorded:" "instances" "threads" "rate/sec"
	for _loop_instances in ${SH_ARG_INSTANCES}; do
		echo "-----------------------------------------"
		for _loop_threads in ${SH_ARG_GROUPS}; do
			_var_rate="$(sh_measure "${_loop_instances}" "${_loop_threads}")"
			sh_log "${SH_ARG_BASE}" "${_loop_instances}" "${_loop_threads}" "${_var_rate}"
			printf " %9s  %9s  %7s  %8s\n" "" "${_loop_instances}" "${_loop_threads}" "${_var_rate}"
		done
	done

	echo "baseline written to ${SH_ARG_BASE}"

	exit 0
fi

_var_base_variant="$(awk '$1 !~ /^#/ { print $1; exit }' "${SH_ARG_BASE}")"
test "${_var_base_variant}" = "${SH_VARIANT}" || {
	echo "ERROR: baseline variant '${_var_base_variant}' does not match the '${SH_VARIANT}' test program, re-record with -R"
	exit 78
}

_var_base_cpu="$(sed -n 's/^# cpu: //p' "${SH_ARG_BASE}" | head -1)"
test "${_var_base_cpu}" = "${SH_CPU}" || {
	echo "WARNING: baseline cpu '${_var_base_cpu}' does not match '${SH_CPU}', the results are probably not comparable"
	echo "WARNING: re-record the baseline with -R"
}

printf " %9s  %7s  %8s  %8s  %9s  %s\n" "instances" "threads" "baseline" "current" "delta_pct" "verdict"

for _loop_instances in ${SH_ARG_INSTANCES}; do
	_var_first_rate=
	_var_last_rate=

	echo "---------------------------------------------------------------"

	for _loop_threads in ${SH_ARG_GROUPS}; do
		_var_base_rate="$(awk -v i="${_loop_instances}" -v t="${_loop_threads}" '$1 !~ /^#/ && $3 == i && $4 == t { print $5; exit }' "${SH_ARG_BASE}")"
		test -n "${_var_base_rate}" || {
			echo "ERROR: no baseline entry for ${_loop_instances} instance(s) and ${_loop_threads} thread(s), re-record with -R"
			exit 78
		}

		_var_rate="$(sh_measure "${_loop_instances}" "${_loop_threads}")"
		sh_log "${SH_ARG_BASE}-${SH_DATE}" "${_loop_instances}" "${_loop_threads}" "${_var_rate}"
		test -n "${_var_first_rate}" || _var_first_rate="${_var_rate}"
		_var_last_rate="${_var_rate}"

		if awk -v c="${_var_rate}" -v b="${_var_base_rate}" -v tol="${SH_ARG_TOL}" \
				'BEGIN { exit !(b > 0 && c >= b * (100 - tol) / 100) }'; then
			_var_verdict="OK"
		else
			_var_verdict="REGRESSION"
			SH_RETVAL="70"
		fi

		_var_delta="$(awk -v c="${_var_rate}" -v b="${_var_base_rate}" 'BEGIN { printf "%+.1f", (b > 0) ? ((c - b) * 100 / b) : 0 }')"
		printf " %9s  %7s  %8s  %8s  %9s  %s\n" "${_loop_instances}" "${_loop_threads}" "${_var_base_rate}" "${_var_rate}" "${_var_delta}" "${_var_verdict}"
	done

	if test "${_var_first_rate}" != "${_var_last_rate}"; then
		if awk -v f="${_var_first_rate}" -v l="${_var_last_rate}" -v s="${SH_ARG_SCALE}" \
				'BEGIN { exit !(f > 0 && l / f >= s) }'; then
			_var_verdict="OK"
		else
			_var_verdict="REGRESSION"
			SH_RETVAL="70"
		fi

		_var_scale="$(awk -v f="${_var_first_rate}" -v l="${_var_last_rate}" 'BEGIN { printf "%.2f", (f > 0) ? (l / f) : 0 }')"
		echo "scaling (${_loop_instances} instance(s)): ${_var_scale} (minimum ${SH_ARG_SCALE}): ${_var_verdict}"
	fi
done

exit ${SH_RETVAL}
