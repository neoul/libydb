#!/bin/sh
. ./util.sh
test_init $0 $1
echo -n "TEST: $TESTNAME : "

r1=`ydb -r local -f ../examples/yaml/yaml-value.yaml -N --read /`
r2=`ydb -r local -f ../examples/yaml/yaml-key-value.yaml -N --read /KEY`
r3=`ydb -r local -f ../examples/yaml/yaml-sequence.yaml -N --read '/seq/Block style/3' --read '/seq/Flow style/3'`
r4=`ydb -r local -f ../examples/yaml/yaml-set.yaml -N --read '/set/baseball players/Sammy Sosa' --read '/set/baseball teams/New York Yankees'`
r5=`ydb -r local -f ../examples/yaml/yaml-map.yaml -N --read '/map/Block style/Oren' --read '/map/Flow style/Clark'`
r6=`ydb -r local -f ../examples/yaml/yaml-omap.yaml -N --read '/omap/Bestiary/anaconda' --read '/omap/Numbers/two'`
r7=`ydb -r local -f ../examples/yaml/yaml-empty-list.yaml -N --read '/0' --read '/2'`

[ "x$r1" != "xvalue-only" ]     && echo " - r1 TEST: failed ($r1)" && exit 1
[ "x$r2" != "xVAL" ]             && echo " - r2 TEST: failed ($r2)" && exit 1
[ "x$r3" != "xMars Mars" ]       && echo " - r3 TEST: failed ($r3)" && exit 1
[ "x$r4" != "x " ]                && echo " - r4 TEST: failed ($r4)" && exit 1
[ "x$r5" != "xBen-Kiki Evans" ]  && echo " - r5 TEST: failed ($r5)" && exit 1
[ "x$r6" != "xSouth-American constrictor snake. Scaly. 2" ] && echo " - r6 TEST: failed ($r6)" && exit 1
[ "x$r7" != "x value2" ]  && echo " - r7 TEST: failed ($r7)" && exit 1

echo "ok"

test_deinit
exit 0

