#!/bin/bash

ydb -r pub -a uss://test -d -f demo.yaml &
PUBPID=$!

./demo
# go run demo.go
kill $PUBPID
cd -
