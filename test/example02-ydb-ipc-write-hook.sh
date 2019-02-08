#!/bin/bash

rm -f /tmp/test

echo
echo + Run ydb-ipc-pub.
ydb-ipc-pub > ydb-ipc-pub.log &

echo
echo + Check unix socket exists as we expected.
netstat -ap -x | grep /tmp/test

echo
echo + Run ydb-ipc-write-hook
ydb-ipc-write-hook > ydb-ipc-write-hook.log
ydb-ipc-write-hook supressed > ydb-ipc-write-hook-supressed.log

killall -2 ydb-ipc-pub

sleep 1

echo
echo + ydb-ipc-pub\'s data block
cat ydb-ipc-pub.log

echo
echo + ydb-ipc-write-hook\'s log
cat ydb-ipc-write-hook.log

echo
echo + ydb-ipc-write-hook \(supressed\)\'s log
cat ydb-ipc-write-hook-supressed.log