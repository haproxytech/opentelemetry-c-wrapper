#!/bin/sh -x
#
# test-rebase.sh by Miroslav Zagorac <mzagorac@haproxy.com>
#
# Walk through a git rebase one commit at a time, building each one and stopping
# if compilation warnings or errors appear.  Restores src/.build-counter before
# continuing so the rebase does not trip over the auto-incremented counter.
#
      SH_LOG="_log-$(basename "${0}")"
SH_CONF_OPTS="--enable-warnings"
 SH_CMD_TEST="make-show-warnings.sh"
    _var_cnt="1"

command -v "${SH_CMD_TEST}" || SH_CONF_OPTS= SH_CMD_TEST="grep -w error:"


./scripts/distclean
./scripts/bootstrap
./scripts/configure --enable-debug ${SH_CONF_OPTS}

while true; do
	_var_log="${SH_LOG}-${_var_cnt}"
	_var_cnt="$((_var_cnt + 1))"

	make clean > "${_var_log}"
	make -j8 $(grep -q "^test:" Makefile.am && echo "test") 2>&1 | tee -a "${_var_log}"

	_var_test="$(${SH_CMD_TEST} "${_var_log}")"
	test -n "${_var_test}" && break

	git restore src/.build-counter
	git rebase --continue || break
done
