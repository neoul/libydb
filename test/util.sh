#!/bin/sh

remove_log()
{
    rm -f $TESTNAME.*.log
}

BGLIST=""
remove_bg()
{
    # echo $BG
    for BG in $BGLIST; do
        # echo $BG
        sleep 1
        kill -2 $BG
    done
}

LOGCOUNT=0
run_bg()
{
    # echo $@
    if [ $MEMCHECK -eq 1 ]; then
        LOGCOUNT=`expr $LOGCOUNT + 1`
        # valgrind --log-file=$TESTNAME.memcheck.$LOGCOUNT.log $1 > $2 &
        eval "valgrind --leak-check=full --log-file=$TESTNAME.memcheck.$LOGCOUNT.log $@ &"
        sleep 1
        LAST_PID=$(head -n 1 $TESTNAME.memcheck.$LOGCOUNT.log | tr -cd '[[:digit:]]')
    else
        # echo $@
        # $1 > $2 &
        eval "$@ &"
        LAST_PID=$!
        sleep 1
    fi
    if [ "x$LAST_PID" != "x" ]; then
        BGLIST="$LAST_PID $BGLIST"
    fi
}

run_fg()
{
    # echo $@
    if [ $MEMCHECK -eq 1 ]; then
        LOGCOUNT=`expr $LOGCOUNT + 1`
        # echo "valgrind --log-file=$TESTNAME.memcheck.$LOGCOUNT.log '$1 > $2'"
        eval "valgrind --log-file=$TESTNAME.memcheck.$LOGCOUNT.log $@"
        sleep 1
        return $?
    else
        # /bin/sh -c "$1 > $2"
        eval "$@"
        return $?
    fi
}

TESTNAME="none"
MEMCHECK=0
test_init()
{
    TESTNAME=`basename "$1"`
    TESTNAME="${TESTNAME%.*}"
    if [ "x$2" = "xmemcheck" ]; then
        MEMCHECK=1
    fi
    remove_log
}

test_deinit()
{
    remove_bg
    sleep 1
}
