#!/bin/bash

rm -f /tmp/test

echo
echo + Run ydb-ipc-read-hook.
ydb-ipc-read-hook > ydb-ipc-read-hook.log &

echo
echo + Check unix socket exists as we expected.
netstat -ap -x | grep /tmp/test

echo
echo + Run ydb-ipc-read-hook subscriber
ydb-ipc-read-hook subscriber > ydb-ipc-read-hook-subscriber.log

killall -2 ydb-ipc-read-hook

sleep 1

echo
echo + ydb-ipc-read-hook\'s data block
cat ydb-ipc-read-hook.log

echo
echo + ydb-ipc-read-hook subscriber\'s data block
cat ydb-ipc-read-hook-subscriber.log

echo
echo + Check if their data block differ.
diff -q ydb-ipc-read-hook.log ydb-ipc-read-hook-subscriber.log