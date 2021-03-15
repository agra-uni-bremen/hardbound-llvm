#!/bin/sh
set -e

# Change to directory of script.
cd "${0%/*}"

retval=0
find . -type f \( -perm -u=x -o -perm -g=x -o -perm -o=x \) | while read f; do
	[ "${f##*/}" = "${0##*/}" ] && continue
	printf "Running test case %s: " "${f##*/}"

	reg=$(symex-vp --intercept-syscalls "${f}" 2>/dev/null | awk -F' = ' '/^a6/ { print $2 }')
	if [ "${reg}" != "(none, 2342)" ]; then
		printf "FAIL [expected %s - got %s]\n" "(none, 2342)" "${reg}"
		retval=1
		continue
	fi

	printf "OK\n"
done

exit ${retval}
