#!/bin/bash

ydb -r sub -w -a uss://test --file demo.yaml
ydb -r sub -w -a uss://test --write /ydb/hello/world
sleep 2
ydb -r sub -w -a uss://test --write /ydb/hello/demon
sleep 2
ydb -r sub -w -a uss://test --write /ydb/hello/demon=ok
sleep 2
ydb -r sub -w -a uss://test --write /ydb/hello/world=hi
sleep 2
ydb -r sub -w -a uss://test --delete /ydb/hello/world
sleep 2
ydb -r sub -w -a uss://test --delete /ydb/hello
sleep 2