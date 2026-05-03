#!/bin/sh -u
#
# speed.sh by Miroslav Zagorac <mzagorac@haproxy.com>
#
SH_ARG_RUNTIME="60000"
SH_ARG_RUNTYPE="dev_null"
   SH_ARG_INST="1"
        SH_DIR="$(dirname "${0}")"
       SH_TEST="${SH_DIR}/otel-c-wrapper-test"
        SH_CFG="${SH_DIR}/otel-cfg.yml"
        SH_CTX="speed_test"
        SH_LOG="_log_speed-$(date +%Y%m%d-%H%M%S)"
    SH_PIDFILE="_pid_speed"
     SH_ERRLOG="${SH_LOG}-dropped"
     SH_STDERR="${SH_LOG}-stderr"
  SH_USAGE_MSG="usage: $(basename "${0}") [-i instances] [-r runtime] [-t runtype]"


sh_run_count()
{
	local _arg_threads="${1}"
	local _var_drop_cnt=

	mkfifo "${SH_STDERR}"
	wc -l < "${SH_STDERR}" > "${SH_ERRLOG}" &

	${SH_TEST} -D0 -c "${SH_CFG}" -i "${SH_ARG_INST}" -n "${SH_CTX}" -p "${SH_PIDFILE}" -r${SH_ARG_RUNTIME} -t${_arg_threads} -s1 2>"${SH_STDERR}" | tee -a "${SH_LOG}"
	wait
	_var_drop_cnt="$(cat "${SH_ERRLOG}")"
	echo "Total spans dropped: ${_var_drop_cnt}"
	echo

	rm "${SH_STDERR}" "${SH_ERRLOG}"
}

sh_run_dev_null()
{
	local _arg_threads="${1}"

	${SH_TEST} -D0 -c "${SH_CFG}" -i "${SH_ARG_INST}" -n "${SH_CTX}" -p "${SH_PIDFILE}" -r${SH_ARG_RUNTIME} -t${_arg_threads} -s1 2>/dev/null | tee -a "${SH_LOG}"
}


while getopts i:r:t: c; do
	case "${c}" in
	  i)	SH_ARG_INST="${OPTARG}" ;;
	  r)	SH_ARG_RUNTIME="${OPTARG}" ;;
	  t)	SH_ARG_RUNTYPE="${OPTARG}" ;;
	  \?)	echo "${SH_USAGE_MSG}"; exit 64 ;;
	esac
done

shift $((OPTIND - 1))

test -x "${SH_TEST}" || {
	echo "${SH_TEST}: missing or not executable"
	exit 1
}
test -e "${SH_PIDFILE}" && {
	echo "${SH_PIDFILE}: pid file already exists"
	exit 2
}

for _var_threads in 1 2 4 8 16 32 64 128 256 512 1024 2048 4096; do
	sh_run_${SH_ARG_RUNTYPE} "${_var_threads}"
	rm -f "${SH_PIDFILE}"
done
