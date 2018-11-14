#!/bin/sh
. ./util.sh
test_init $0 $1
echo -n "TEST: $TESTNAME : "
run_bg ydb -r pub -d -s -N -f ../examples/yaml/ydb-sample.yaml > $TESTNAME.PUB.log

r1=`ydb -r sub -N --unsubscribe --sync-before-read --read /2/2-1/2-1-1`
r2=`ydb -r sub -N --unsubscribe --sync-before-read --read /1/1-2/1-2-3`

test_deinit

if [ "value_$r1" =  "value_v7" ];then
    if [ "value_$r2" =  "value_v6" ];then
        echo "ok ($r1, $r2)"
        exitcode=0
    else
        echo "failed ($r1, $r2)"
        exitcode=1
    fi
    
else
    echo "failed ($r1, $r2)"
    exitcode=1
fi
exit $exitcode