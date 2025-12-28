#!/bin/sh -u
#
# speed.sh by Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_ARG_RUNTIME="${1:-60000}"
SH_ARG_RUNTYPE="${2:-dev_null}"
        SH_DIR="$(dirname "${0}")"
        SH_CFG="${SH_DIR}/speed-cfg.yml"
        SH_LOG="_log_speed-$(date +%Y%m%d-%H%M%S)"
     SH_ERRLOG="${SH_LOG}-dropped"
     SH_STDERR="${SH_LOG}-stderr"
       SH_TEST="/opt/bin/otel-c-wrapper-test -c /opt/share/doc/opentelemetry-c-wrapper/otel-cfg.yml"

test -x "${SH_DIR}/otel-c-wrapper-test" && SH_TEST="${SH_DIR}/otel-c-wrapper-test"


sh_run_count()
{
	local _arg_threads="${1}"
	local _var_drop_cnt=

	mkfifo "${SH_STDERR}"
	wc -l < "${SH_STDERR}" > "${SH_ERRLOG}" &

	${SH_TEST} -D0 -c "${SH_CFG}" -r${SH_ARG_RUNTIME} -t${_arg_threads} -s1 2>"${SH_STDERR}" | tee -a "${SH_LOG}"
	wait
	_var_drop_cnt="$(cat "${SH_ERRLOG}")"
	echo "Total spans dropped: ${_var_drop_cnt}"
	echo

	rm "${SH_STDERR}" "${SH_ERRLOG}"
}

sh_run_dev_null()
{
	local _arg_threads="${1}"

	${SH_TEST} -D0 -c "${SH_CFG}" -r${SH_ARG_RUNTIME} -t${_arg_threads} -s1 2>/dev/null | tee -a "${SH_LOG}"
}


for _var_threads in 1 2 4 8 16 32 64 128 256 512 1024; do
	sh_run_${SH_ARG_RUNTYPE} "${_var_threads}"
done
