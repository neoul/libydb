#!/bin/sh
mode=$1

rm -f memcheck*.log
rm -f ydb.*.log

if [ "x$mode" = "xmemcheck" ]; then
    RUN_HEAD="valgrind --log-file=memcheck.log"
fi

TESTNAME=`basename "$0"`
echo -n "TEST: $TESTNAME : "

$RUN_HEAD ydb -r pub -d -s -N -f ../examples/yaml/ydb-sample.yaml -f ../examples/yaml/ydb-list.yaml > ydb.pub.log &

if [ "x$mode" = "xmemcheck" ]; then
    sleep 1
    publisher=$(head -n 1 memcheck.log | tr -cd '[[:digit:]]')
else
    publisher=$!
    sleep 1
fi

ydb -r sub -N -s > ydb.sub.log
sleep 1

kill -2 $publisher
sleep 1

result=`diff -q ydb.pub.log ydb.sub.log`
if [ "x$result" =  "x" ];then
    echo "ok"
    exitcode=0
else
    echo "failed"
    echo 
    diff ydb.pub.log ydb.sub.log
    echo
    exitcode=1
fi

exit $exitcode