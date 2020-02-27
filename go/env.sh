#!/bin/bash
source ~/.bashrc
# echo $BASH_SOURCE
SOURCEPATH=$(dirname "$(readlink -f $BASH_SOURCE)")
# echo $SOURCEPATH
export GOPATH=$SOURCEPATH
export GOBIN=$GOPATH/bin
PATH1=${PATH%:${GOPATH}}
export PATH=${PATH1}:${GOPATH}
PATH1=${PATH%:${GOBIN}}
export PATH=${PATH1}:${GOBIN}
