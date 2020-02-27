#!/bin/bash

ydb -r pub -a uss://test -d -f ../examples/yaml/yaml-types.yaml &
PUBPID=$!

go run main.go
kill $PUBPID
