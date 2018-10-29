#!/bin/sh
cd test
echo $PWD
find . -type f -exec {} \;
rm ydb.*.log
cd -
