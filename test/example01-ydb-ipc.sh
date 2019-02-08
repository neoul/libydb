#!/bin/bash

rm -f /tmp/test

echo
echo + Run ydb-ipc-pub.
ydb-ipc-pub > ydb-ipc-pub.log &

echo
echo + Check unix socket exists as we expected.
netstat -ap -x | grep /tmp/test

echo
echo + Run ydb-ipc-sub
ydb-ipc-sub > ydb-ipc-sub.log

killall -2 ydb-ipc-pub

sleep 1

echo
echo + ydb-ipc-pub\'s data block
cat ydb-ipc-pub.log

echo
echo + ydb-ipc-sub\'s data block
cat ydb-ipc-sub.log

echo
echo + Check if their data block differ.
diff -q ydb-ipc-pub.log ydb-ipc-sub.log