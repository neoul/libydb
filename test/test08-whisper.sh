#!/bin/sh
. ./util.sh
test_init $0 $1
echo -n "TEST: $TESTNAME : "
run_bg "ydb -r pub -d -a uss://top -s -N > $TESTNAME.PUB1.log"
run_bg "ydb -r pub -d -a uss://top -s -N > $TESTNAME.PUB2.log"

MEMCHECK=0
run_bg 'echo \\n\
message: \\n\
 n1: msg1\n | ydb-whisper-example uss://top 1 2 > $TESTNAME.PUB3.log'

run_bg 'echo \\n\
message: \\n\
 n2: msg2\\n | ydb-whisper-example uss://top 2 1 > $TESTNAME.PUB4.log'

test_deinit

sleep 1
r1=`diff $TESTNAME.PUB1.log $TESTNAME.PUB2.log`
r2=`cat $TESTNAME.PUB3.log | grep ydb_whisper_merge | cut -d' ' -f1`
r3=`cat $TESTNAME.PUB4.log | grep ydb_whisper_merge | cut -d' ' -f1`
sub='ydb_whisper_merge'

if [ "value_$r1" =  "value_" ];then
    if [ "value_$r2" =  "value_$r3" ];then
        echo "ok ($r1, $r2, $r3)"
        exitcode=0
    else
        echo "failed ($r1, $r2, $r3)"
        exitcode=1
    fi
    
else
    echo "ok ($r1, $r2, $r3)"
    exitcode=1
fi
exit $exitcode