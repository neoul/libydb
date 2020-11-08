#!/bin/bash

ydb -r sub -w -a uss://test --file demo.yaml
ydb -r sub -w -a uss://test --file list.yaml
ydb -r sub -w -a uss://test --write /ydb/hello/world
sleep 1
ydb -r sub -w -a uss://test --write /ydb/hello/demon
sleep 1
ydb -r sub -w -a uss://test --write /ydb/hello/demon=ok
sleep 1
ydb -r sub -w -a uss://test --write /ydb/hello/world=hi
sleep 1
ydb -r sub -w -a uss://test --delete /ydb/hello/world
sleep 1
ydb -r sub -w -a uss://test --delete /ydb/hello
sleep 1
ydb -r sub -a uss://test --print /