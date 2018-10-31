#!/bin/sh
. ./util.sh
test_init $0 $1
echo -n "TEST: $TESTNAME : "
run_bg ydb -r pub -d -s -N -f ../examples/yaml/ydb-sample.yaml > $TESTNAME.PUB.log
run_fg ydb -r sub -N -s > $TESTNAME.SUB.log
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
