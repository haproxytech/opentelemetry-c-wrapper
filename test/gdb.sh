#!/bin/sh
#
# gdb.sh by Miroslav Zagorac <mzagorac@haproxy.com>
#
 SH_DIR="$(dirname "${0}")"
SH_TEST="${SH_DIR}/.libs/otel-c-wrapper-test"

sh_gdb_run ()
{
	echo
	echo "run -c test/otel-cfg.yml -R1 -t1"
	echo

	LD_LIBRARY_PATH="${SH_DIR}/../src/.libs" gdb "${SH_TEST}"
}

sh_gdb_core ()
{
	LD_LIBRARY_PATH="${SH_DIR}/../src/.libs" gdb -core core "${SH_TEST}"
}


test -x "${SH_TEST}" || SH_TEST="${SH_TEST}_dbg"

if test -f core; then
	sh_gdb_core
else
	sh_gdb_run
fi
