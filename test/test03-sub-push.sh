#!/bin/sh
. ./util.sh
test_init $0 $1
echo -n "TEST: $TESTNAME : "
run_bg "ydb -n Y -r pub -a uss://test -d -s > $TESTNAME.PUB.log"
run_fg "ydb -n Y -r sub -s -w -a uss://test -f ../examples/yaml/ydb-sample.yaml > $TESTNAME.SUB.log"
test_deinit

RESULT=`diff -q $TESTNAME.PUB.log $TESTNAME.SUB.log`
if [ "x$RESULT" =  "x" ];then
    echo "ok"
    exitcode=0
else
    echo "failed"
    echo 
    diff $TESTNAME.PUB.log $TESTNAME.SUB.log
    echo
    exitcode=1
fi
exit $exitcode
