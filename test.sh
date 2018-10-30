#!/bin/sh
cd test
echo $PWD
rm -f ydb.*.log
find . -type f -exec {} \;
rm -f ydb.*.log
cd -
