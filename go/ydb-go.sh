#!/bin/bash

ydb -r pub -a uss://test -d -f ../examples/yaml/yaml-types.yaml &
PUBPID=$!

cd demo
go run demo.go
kill $PUBPID
cd -