#!/bin/sh
. ./util.sh
test_init $0 $1
echo -n "TEST: $TESTNAME : "

r1=`ydb -f ../examples/yaml/yaml-value.yaml --read /`
r2=`ydb -f ../examples/yaml/yaml-key-value.yaml --read /KEY`
r3=`ydb -f ../examples/yaml/yaml-sequence.yaml --read '/seq/Block style/3' --read '/seq/Flow style/3'`
r4=`ydb -f ../examples/yaml/yaml-set.yaml --read '/set/baseball players/Sammy Sosa' --read '/set/baseball teams/New York Yankees'`
r5=`ydb -f ../examples/yaml/yaml-map.yaml --read '/map/Block style/Oren' --read '/map/Flow style/Clark'`
r6=`ydb -f ../examples/yaml/yaml-omap.yaml --read '/omap/Bestiary/anaconda' --read '/omap/Numbers/two'`
r7=`ydb -f ../examples/yaml/yaml-empty-list.yaml --read '/0' --read '/2'`
r8=`ydb -f ../examples/yaml/yaml-anchor-reference1.yaml --read '/foo/K1'`
r9=`ydb -f ../examples/yaml/yaml-anchor-reference2.yaml --read '/5/step/instrument'`
r10=`ydb -f ../examples/yaml/yaml-anchor-reference3.yaml --read '/merge/5/r'`
r13=`ydb -f ../examples/yaml/ydb-write.yaml -f ../examples/yaml/ydb-delete.yaml --read '/system/fan/fan[1]/current_speed'`

ydb -n YDB -f ../examples/yaml/yaml-reference-card.yaml -s > r11.yaml

ydb -n YDB -f r11.yaml -s > r12.yaml
r11=`diff r11.yaml r12.yaml`

[ "x$r1" != "xvalue-only" ]     && echo " - r1 TEST: failed ($r1)" && exit 1
[ "x$r2" != "xVAL" ]             && echo " - r2 TEST: failed ($r2)" && exit 1

r3=$(printf "$r3" | tr '\n' ' ')
[ "x$r3" != "xMars Mars" ]       && echo " - r3 TEST: failed ($r3)" && exit 1
r4=$(printf "$r4" | tr '\n' ' ')
[ "x$r4" != "x" ]                && echo " - r4 TEST: failed ($r4)" && exit 1
r5=$(printf "$r5" | tr '\n' ' ')
[ "x$r5" != "xBen-Kiki Evans" ]  && echo " - r5 TEST: failed ($r5)" && exit 1
r6=$(printf "$r6" | tr '\n' ' ')
[ "x$r6" != "xSouth-American constrictor snake. Scaly. 2" ] && echo " - r6 TEST: failed ($r6)" && exit 1
r7=$(printf "$r7" | tr '\n' ' ')
[ "x$r7" != "x value2" ]  && echo " - r7 TEST: failed ($r7)" && exit 1
r8=$(printf "$r8" | tr '\n' ' ')
[ "x$r8" != "xOne" ]  && echo " - r8 TEST: failed ($r8)" && exit 1
r9=$(printf "$r9" | tr '\n' ' ')
[ "x$r9" != "xLasik 2000" ]  && echo " - r9 TEST: failed ($r9)" && exit 1
r10=$(printf "$r10" | tr '\n' ' ')
[ "x$r10" != "x10" ]  && echo " - r10 TEST: failed ($r10)" && exit 1
r11=$(printf "$r11" | tr '\n' ' ')
[ "x$r11" != "x" ]  && echo " - r11 TEST: failed ($r11)" && exit 1
echo "ok"
[ "x$r13" = "x100" ]             && echo " - r13 TEST: failed ($r2)" && exit 1

test_deinit
exit 0

