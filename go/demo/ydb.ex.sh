#!/bin/bash

ydb -r pub -a uss://test -d -f ydb.ex.yaml &
PUBPID=$!

# cd ydb.ex
go run ydb.ex.go
kill $PUBPID
cd -