#!/bin/sh
. ./util.sh
test_init $0 $1
echo -n "TEST: $TESTNAME : "
run_bg ydb -r pub -d -s -a uss://test -f ../examples/yaml/ydb-sample.yaml -f ../examples/yaml/ydb-list.yaml > $TESTNAME.PUB.log
run_fg ydb -r sub -s -w -a uss://test --delete /2/2-1/2-1-1 > $TESTNAME.SUB.log
test_deinit

RESULT=`diff -w -q $TESTNAME.PUB.log $TESTNAME.SUB.log`
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
