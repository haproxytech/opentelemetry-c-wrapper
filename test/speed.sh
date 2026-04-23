#!/bin/sh -u
#
# speed.sh by Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_ARG_RUNTIME="${1:-60000}"
SH_ARG_RUNTYPE="${2:-dev_null}"
        SH_DIR="$(dirname "${0}")"
       SH_TEST="${SH_DIR}/otel-c-wrapper-test"
        SH_CFG="${SH_DIR}/speed-cfg.yml"
        SH_LOG="_log_speed-$(date +%Y%m%d-%H%M%S)"
    SH_PIDFILE="_pid_speed"
     SH_ERRLOG="${SH_LOG}-dropped"
     SH_STDERR="${SH_LOG}-stderr"


sh_run_count()
{
	local _arg_threads="${1}"
	local _var_drop_cnt=

	mkfifo "${SH_STDERR}"
	wc -l < "${SH_STDERR}" > "${SH_ERRLOG}" &

	${SH_TEST} -D0 -c "${SH_CFG}" -p "${SH_PIDFILE}" -r${SH_ARG_RUNTIME} -t${_arg_threads} -s1 2>"${SH_STDERR}" | tee -a "${SH_LOG}"
	wait
	_var_drop_cnt="$(cat "${SH_ERRLOG}")"
	echo "Total spans dropped: ${_var_drop_cnt}"
	echo

	rm "${SH_STDERR}" "${SH_ERRLOG}"
}

sh_run_dev_null()
{
	local _arg_threads="${1}"

	${SH_TEST} -D0 -c "${SH_CFG}" -p "${SH_PIDFILE}" -r${SH_ARG_RUNTIME} -t${_arg_threads} -s1 2>/dev/null | tee -a "${SH_LOG}"
}


test -x "${SH_TEST}" || {
	echo "${SH_TEST}: missing or not executable"
	exit 1
}
test -e "${SH_PIDFILE}" && {
	echo "${SH_PIDFILE}: pid file already exists"
	exit 2
}

for _var_threads in 1 2 4 8 16 32 64 128 256 512 1024; do
	sh_run_${SH_ARG_RUNTYPE} "${_var_threads}"
	rm -f "${SH_PIDFILE}"
done
