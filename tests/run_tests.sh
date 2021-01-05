#!/bin/sh
set -e

# Change to directory of script.
cd "${0%/*}"

retval=0
find . -type f \( -perm -u=x -o -perm -g=x -o -perm -o=x \) | while read f; do
	[ "${f##*/}" = "${0##*/}" ] && continue
	printf "Running test case %s: " "${f##*/}"

	reg=$(tiny32-vp --intercept-syscalls "${f}" 2>/dev/null | awk '/^a6/ { print $4 }')
	if [ "${reg}" -ne 2342 ]; then
		printf "FAIL [expected %d - got %d]\n" "2342" "${reg}\n"
		retval=1
		continue
	fi

	printf "OK\n"
done

exit ${retval}
