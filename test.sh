#!/bin/sh

cd test
TESTLIST=`find . -name "test*.sh" -type f | sort`
for ENTRY in $TESTLIST; do
    # echo $ENTRY
    $ENTRY $@
    ret=$?
    if [ $ret -ne 0 ]; then
        cd -
        exit 1
    fi
done
cd -
