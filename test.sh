#!/bin/sh

cd test
TESTLIST=`find . -name "test*.sh" -type f | sort`
for ENTRY in $TESTLIST; do
    # echo $ENTRY
    $ENTRY
done
cd -
