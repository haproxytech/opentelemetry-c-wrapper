#!/bin/sh -u
#
# sanitize.sh by Miroslav Zagorac <mzagorac@haproxy.com>
#
# Build the library under the sanitizers and run the test suites against it.
# Each pass snapshots the source tree into its own build directory, so the
# source tree is never written to and its own build stays configured; the
# snapshot leaves the build artifacts behind so no object built without
# instrumentation can survive into the sanitized library.
#
# Without options the address and the undefined behavior passes run, both of
# which are expected to report nothing.  The thread sanitizer pass has to be
# asked for, because the installed OpenTelemetry libraries are not instrumented:
# the sanitizer cannot see the synchronization inside them, so every handoff
# between the wrapper and an SDK worker thread, and every SDK object the wrapper
# only constructs, is reported as a race.  Its findings are worth reading one by
# one, but they are not a gate until the SDK itself is built with the sanitizer.
#
#   Usage: ./scripts/sanitize.sh [-a] [-t] [-u]
#
#     -a  run the address sanitizer pass
#     -t  run the thread sanitizer pass
#     -u  run the undefined behavior sanitizer pass
#
     SH_SRCDIR="$(realpath "$(dirname "${0}")/..")"
     SH_LIBDIR="/opt"
   SH_BUILDDIR="${TMPDIR:-/tmp}/otelc-sanitize"
      SH_TESTS="otel-c-wrapper-test_dbg test-yaml_dbg test-tracer_dbg test-meter_dbg test-logger_dbg test-multi_dbg"
    SH_REPORTS="ERROR: AddressSanitizer|ERROR: LeakSanitizer|WARNING: ThreadSanitizer|runtime error:|SUMMARY: UndefinedBehaviorSanitizer"
    SH_FAILURES="0"
      SH_NOASLR="no"
SH_ARG_SANITIZERS=""


usage ()
{
	echo "Usage: ${0} [-a] [-t] [-u]"

	exit 1
}


# The thread sanitizer refuses to start when the kernel randomizes more address
# bits than its shadow mapping allows, which is the default on recent kernels.
# Lowering vm.mmap_rnd_bits would need root, so the programs it instruments are
# started with randomization turned off instead.
sanitize_exec ()
{
	if test "${SH_NOASLR}" = "yes"; then
		setarch "$(uname -m)" -R "${@}"
	else
		"${@}"
	fi
}


sanitize_build ()
{
	_arg_san="${1}"
	_var_treedir="${SH_BUILDDIR}/${_arg_san}"

	rm -rf "${_var_treedir}"
	mkdir -p "${_var_treedir}" || return 73
	tar cf - -C "${SH_SRCDIR}" \
		--anchored --exclude=./configure --exclude=./config --exclude=./config.status \
		--exclude=./config.log --exclude=./aclocal.m4 --exclude=./libtool \
		--exclude=./stamp-h1 --exclude=./autom4te.cache \
		--no-anchored --exclude=.git --exclude=.libs --exclude=.deps \
		--exclude=Makefile --exclude=Makefile.in --exclude=core \
		--exclude='*.o' --exclude='*.lo' --exclude='*.la' --exclude='*.so*' \
		--exclude='*~' --exclude='_*' . | tar xf - -C "${_var_treedir}" || return 74

	(
		cd "${_var_treedir}" || exit 71

		sh scripts/bootstrap > "${_var_treedir}/_log-bootstrap" 2>&1 || exit 70

		CFLAGS="-O1" CXXFLAGS="-O1" \
			sanitize_exec ./configure --prefix="${SH_LIBDIR}" \
				--enable-debug --enable-warnings "--enable-${_arg_san}" \
				> "${_var_treedir}/_log-configure" 2>&1 || exit 78

		make -j8 > "${_var_treedir}/_log-make" 2>&1 || exit 70
		make -C test check -j8 >> "${_var_treedir}/_log-make" 2>&1 || exit 70
	)
}


sanitize_run ()
{
	_arg_san="${1}"
	_var_treedir="${SH_BUILDDIR}/${_arg_san}"
	_var_failed="0"

	# The raw binaries are used so that the snapshot library is loaded
	# ahead of the installed one, whatever the wrapper scripts prepend.
	for _loop_test in ${SH_TESTS}; do
		_var_log="${_var_treedir}/_log-${_loop_test}"

		ASAN_OPTIONS="detect_leaks=1:detect_stack_use_after_return=1" \
		TSAN_OPTIONS="halt_on_error=0:second_deadlock_stack=1" \
		UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0" \
		LD_LIBRARY_PATH="${_var_treedir}/src/.libs:${SH_LIBDIR}/lib" \
			sanitize_exec "${_var_treedir}/test/.libs/${_loop_test}" > "${_var_log}" 2>&1

		_var_status="${?}"
		_var_result="$(grep -cE "${SH_REPORTS}" "${_var_log}")"
		_var_tests="$(grep -E '^.*--- Results' "${_var_log}" | tail -n 1)"

		if test ${_var_status} -ne 0 || test ${_var_result} -gt 0; then
			_var_failed="$((_var_failed + 1))"

			echo "  FAIL ${_loop_test}: exit ${_var_status}, ${_var_result} report(s), see ${_var_log}"
		else
			echo "  PASS ${_loop_test}: ${_var_tests:-no assertion summary}"
		fi
	done

	return ${_var_failed}
}


sanitize_pass ()
{
	_arg_san="${1}"

	SH_NOASLR="no"

	echo "*** ${_arg_san} pass ***"

	if test "${_arg_san}" = "tsan"; then
		if command -v setarch > /dev/null 2>&1; then
			SH_NOASLR="yes"
		else
			echo "  WARNING: setarch not found, the pass may not start"
		fi

		echo "  NOTE: the installed OpenTelemetry libraries are not instrumented,"
		echo "        so races reported against them are expected; read each one"
	fi

	if ! sanitize_build "${_arg_san}"; then
		echo "  FAIL unable to build, see ${SH_BUILDDIR}/${_arg_san}/_log-*"

		SH_FAILURES="$((SH_FAILURES + 1))"

		return
	fi

	sanitize_run "${_arg_san}"
	_var_failed="${?}"

	# The thread sanitizer pass only reports; see the note above.
	if test "${_arg_san}" != "tsan"; then
		SH_FAILURES="$((SH_FAILURES + _var_failed))"
	fi
}


while getopts "atuh" c; do
	case "${c}" in
		a) SH_ARG_SANITIZERS="${SH_ARG_SANITIZERS} asan" ;;
		t) SH_ARG_SANITIZERS="${SH_ARG_SANITIZERS} tsan" ;;
		u) SH_ARG_SANITIZERS="${SH_ARG_SANITIZERS} ubsan" ;;
		*) usage ;;
	esac
done

test -z "${SH_ARG_SANITIZERS}" && SH_ARG_SANITIZERS="asan ubsan"

for _loop_san in ${SH_ARG_SANITIZERS}; do
	sanitize_pass "${_loop_san}"
done

echo "*** ${SH_FAILURES} failure(s) ***"

test ${SH_FAILURES} -eq 0
