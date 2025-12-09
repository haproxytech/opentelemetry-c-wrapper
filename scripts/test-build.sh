#!/bin/sh
#
# test-build.sh by Miroslav Zagorac <mzagorac@haproxy.com>
#
# Test all compile-time configuration combinations by reading the option names
# from configuration.h and building the project with every permutation of -D/-U
# flags.  A full distclean/bootstrap/configure cycle is run first, then each
# combination is compiled.
#
SH_DIR="$(dirname "${0}")"


compile_combinations ()
{
	local _var_n=$((1 << ${#}))
	local _loop_i

	for _loop_i in $(seq 0 $((_var_n - 1))); do
		local _loop_param=
		local _var_option=
		local _var_cppflags=
		local _var_shift=0

		for _loop_param in "${@}"; do
			test $(((_loop_i >> _var_shift) & 1)) -eq 1 && _var_option="D" || _var_option="U"
			_var_cppflags="${_var_cppflags:+${_var_cppflags} }-${_var_option}${_loop_param}"
			_var_shift=$((_var_shift + 1))
		done

		make clean >/dev/null 2>&1
		date "+%F %T"
		echo "[${_loop_i}] CPPFLAGS='${_var_cppflags}'"
		make CPPFLAGS="-DOTELC_USER_CONFIG ${_var_cppflags}" -j8
		echo
		./test/otel-c-wrapper-test*
	done
}


"${SH_DIR}/distclean"
"${SH_DIR}/bootstrap"
"${SH_DIR}/configure" "${@}"
compile_combinations $(awk '/OTELC_USER_CONFIG/ { a=1 } /endif/ { a=0 } { if (a>1) print $3; if (a>0) a++ }' include/configuration.h)
