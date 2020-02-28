#!/bin/bash
source ~/.bashrc
# echo $BASH_SOURCE
SOURCEPATH=$(dirname "$(readlink -f $BASH_SOURCE)")
# echo $SOURCEPATH
export GOPATH=$SOURCEPATH
export GOBIN=$HOME/go/bin
PATH1=${PATH%:${GOBIN}}
export PATH=${PATH1}:${GOBIN}
