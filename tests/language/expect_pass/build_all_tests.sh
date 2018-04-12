#!/bin/sh

for i in $(cat ./test_list.txt)
do
	../../../bin/kc -o ${i}.x ${i}.lc
done
