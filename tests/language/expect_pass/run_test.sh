#!/bin/sh
test_output="$(./${1}.x)"
test_rc=$?
if [ "${test_rc}" = '0' ] && [ -e ${1}.expected ] && [ "${test_output}" = "$(cat ./${1}.expected)" ]
then
	echo -n "PASS"
else
	echo -n "FAIL"
fi
