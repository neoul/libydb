#!/bin/bash

#ydb -r pub -a uss://test -d -f demo.yaml &
go run demo.go &
PUBPID=$!
echo $PUBPID
# ./demo
# go run demo.go
sleep 1
ydb -r pub -a uss://test --file ../../examples/yaml/yaml-sequence.yaml
kill -SIGINT $PUBPID
cd -
