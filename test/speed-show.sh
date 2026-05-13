#!/bin/sh -u
#
# Groups all "K worker(s) total count: COUNT, RATE/sec" lines by K and
# prints min/max/total of COUNT and RATE for every group.
#
    SH_NAME="$(basename "${0}")"
_flag_first="true"
 _loop_file=


test ${#} -eq 0 && {
	echo
	echo "usage: ${SH_NAME} file ..."
	echo
	exit 1
}

for _loop_file in "${@}"; do
	test -r "${_loop_file}" || {
		echo "${SH_NAME}: cannot read '${_loop_file}'"
		continue
	}

	test -n "${_flag_first}" || echo
	echo "==> ${_loop_file} <=="
	_flag_first=

	awk '
	/worker\(s\) total count:/ {
		k    = $1 + 0
		cnt  = $5 + 0	# "COUNT," -> numeric strips the comma
		rate = $6 + 0	# "RATE/sec" -> numeric strips the suffix

		if (!(k in runs)) {
			cmin[k] = cmax[k] = cnt
			rmin[k] = rmax[k] = rate
		} else {
			if (cnt  < cmin[k]) cmin[k] = cnt
			if (cnt  > cmax[k]) cmax[k] = cnt
			if (rate < rmin[k]) rmin[k] = rate
			if (rate > rmax[k]) rmax[k] = rate
		}

		ctot[k] += cnt
		rtot[k] += rate
		runs[k] += 1
	}

	END {
		printf "%6s %5s %12s %12s %14s %12s %12s %14s\n", "group", "runs", "count_min", "count_max", "count_total", "rate_min", "rate_max", "rate_total"

		n = 0
		for (k in runs) keys[n++] = k
		for (i = 0; i < n - 1; i++)
			for (j = i + 1; j < n; j++)
				if ((keys[i] + 0) > (keys[j] + 0)) {
					t = keys[i]; keys[i] = keys[j]; keys[j] = t
				}

		for (i = 0; i < n; i++) {
			k = keys[i]
			printf "%6d %5d %12d %12d %14d %12.2f %12.2f %14.2f\n", k, runs[k], cmin[k], cmax[k], ctot[k], rmin[k], rmax[k], rtot[k]
		}
	}
	' "${_loop_file}"
done
