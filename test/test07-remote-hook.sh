#!/bin/sh
. ./util.sh
test_init $0 $1
echo -n "TEST: $TESTNAME : "
run_bg "ydb -r pub -d -f ../examples/yaml/ydb-sample.yaml -v info > $TESTNAME.PUB1.log"
run_bg "ydb-hook-example 1 > $TESTNAME.PUB2.log"
run_bg "ydb-hook-example 2 > $TESTNAME.PUB3.log"
sleep 1
r1=`ydb -r sub --unsubscribe --sync-before-read --read /ge1/enabled`
r2=`ydb -r sub --unsubscribe --sync-before-read --read /ge1/enabled`
r3=`ydb -r sub --unsubscribe --sync-before-read --read /ge2/enabled`
r4=`ydb -r sub --unsubscribe --sync-before-read --read /ge2/enabled`

test_deinit

if [ "value_$r1" =  "value_true" ];then
    if [ "value_$r2" =  "value_false" ];then
        if [ "value_$r3" =  "value_true" ];then
            if [ "value_$r4" =  "value_false" ];then
                echo "ok ($r1, $r2, $r3, $r4)"
                exitcode=0
            else
                echo "failed ($r1, $r2, $r3, $r4)"
                exitcode=1
            fi
            
        else
            echo "failed ($r1, $r2, $r3, $r4)"
            exitcode=1
        fi
    else
        echo "failed ($r1, $r2, $r3, $r4)"
        exitcode=1
    fi
    
else
    echo "failed ($r1, $r2, $r3, $r4)"
    exitcode=1
fi
exit $exitcode