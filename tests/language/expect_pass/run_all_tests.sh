#!/bin/sh

results='test_results.txt'

cat /dev/null >${results}
for i in $(cat test_list.txt)
do
	./run_test.sh ${i} | tee -a ${results}
	echo " ${i}" | tee -a ${results}
done

passes=$(grep ^PASS ${results} | wc -l)
total=$(cat test_list.txt | wc -w)

echo "${passes}/${total} Passed." | tee -a ${results}
