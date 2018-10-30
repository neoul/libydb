#!/bin/sh
# action=$1
TESTNAME=`basename "$0"`
echo -n "TEST: $TESTNAME : "
ydb -r pub -d -s -N > ydb.pub.log &
publisher=$!
sleep 1
cat ../examples/yaml/ydb-sample.yaml | ydb -r sub -N -s -w > ydb.sub.log
kill -2 $publisher

result=`diff -q ydb.pub.log ydb.sub.log`
if [ "x$result" =  "x" ];then
    echo "ok"
    exit 0
else
    echo "failed"
    echo $result
    exit 1
fi