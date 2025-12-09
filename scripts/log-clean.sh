#!/bin/sh -u
#
# Sanitize log output by stripping the leading timestamp (first 16 characters),
# replacing hexadecimal memory addresses with "PTR", and replacing trace IDs
# with "TRACEID".
#
SH_FILE="${1:-_log}"


test -z "${SH_FILE}" && exit 64
test -f "${SH_FILE}" || exit 74

sed '
	s/^.\{16\}//
	s/0x[0-9a-f]*/PTR/g
	s/00-[^-]*-[^-]*-01/TRACEID/g
' "${SH_FILE}"
